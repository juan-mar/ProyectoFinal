#include "tag.h"

#define SAMPLE_INTERVAL 200
#define CAL_TIME 5000
#define CAL_SAMPLES ((CAL_TIME / SAMPLE_INTERVAL) + 2)


static struct_message_t paquete_datos;

static int rssi_display;
static float rssi_filtered;
static float samples[CAL_SAMPLES];
static int barrier = 0;
static bool in_thres = false;

static bool calibrating = false;
static bool detecting = false;
static bool new_msg = false;
static unsigned long lastSampleTime = 0;


static long sumRSSI = 0;
static long sumSqRSSI = 0;
static int sampleCount = 0;
static float sum_var = 0;


static Filtro filtro_kalman;

Receptor::Receptor() {
  threshold = 0;
  varianza = 0;
  //macAddr = 0;
  rssi_display = 0;

}

Receptor::~Receptor() {
  //WiFi.mode(WIFI_OFF);
  //esp_now_deinit();
}

bool Receptor::calibracion() {
  calibrating = true;
  static unsigned long calibStart = millis();
  unsigned long now = millis();

  // Es momento de tomar una nueva muestra?
  //if ((now - lastSampleTime) >= SAMPLE_INTERVAL) 
  if (new_msg && (sampleCount < CAL_SAMPLES))
  {
    new_msg = false;
    Serial.println("calibrando...");
    lastSampleTime = now;

    int rssi_calib = this->getRSSI();
    Serial.print("Muestra RSSI: ");
    Serial.println(rssi_calib);

    sampleCount++;
    sumRSSI += rssi_calib;
    samples[sampleCount - 1] = rssi_calib;
    Serial.print("sampleCount: ");
    Serial.println(sampleCount);
    
    return true;
  }
  else if (sampleCount >= CAL_SAMPLES) {
    Serial.println("Fin calibracion");
    
    calibrating = false;

    if (sampleCount > 0) {
      this -> threshold = (float)sumRSSI / sampleCount;
      for(int i = 0; i < sampleCount; i++) {
        sum_var += ((samples[i] - this->threshold) * (samples[i] - this->threshold))/sampleCount;
      }
      this -> varianza  = sum_var;
      filtro_kalman.set_varianzaR(this -> varianza);
      Serial.print("Threshold:");
      Serial.println(this -> threshold);
      Serial.print("Varianza:");
      Serial.println(this -> varianza);

      barrier = threshold - 3 * sqrt(varianza); //CHECKEAR VALOR DE LA BARRERA
      // Guardar en NVS
      //prefs.begin("calib", false);
      //prefs.putFloat("mean", threshold);
      //prefs.putFloat("var", varianza);
      //prefs.end();


      //notifyCalibrationDone(); // acá le decís al TAG que prenda el LED
    }
    return false; // calibración terminada
  }
  else {
    return true; // calibración en progreso
  }
}

bool Receptor::detect_thres() {
  detecting = true;
  int rssi_curr = rssi_filtered;
  if (new_msg) {
    Serial.print("RSSI actual: ");
    Serial.println(rssi_curr);
    Serial.print("Threshold: ");
    Serial.println(this->threshold);
    Serial.print("Barrier: ");
    Serial.println(barrier);
  }
  if (rssi_curr > this->threshold) {
    in_thres = true;
    return true; // señal detectada
  } 
  else if (rssi_curr < barrier) {
    in_thres = false;
    return false; // señal no detectada
    detecting = false;
  } 
  else if (in_thres) {
    return true; // señal detectada
  }
  else {
    return false; // señal no detectada
    detecting = false;
  }
}


int Receptor::getRSSI() {
  return rssi_display;
}

float Receptor::getThreshold() {
  return threshold;
}

float Receptor::getVarianza() {
  return varianza;
}


/******************* FUNCIONES PARA ESP-NOW *************************/

void promiscuous_rx_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
  // All espnow traffic uses action frames which are a subtype of the mgmnt frames so filter out everything else.
  if (type != WIFI_PKT_MGMT)
    return;

  const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buf;
  const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *)ppkt->payload;
  const wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;

  int rssi = ppkt->rx_ctrl.rssi;
  new_msg = false;
  rssi_display = rssi;

}

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&paquete_datos, incomingData, sizeof(paquete_datos));
  new_msg = true;
  filtro_kalman.filtrado(rssi_display);
  //  Serial.print("Bytes received: ");
  //  Serial.println(len);
  //  Serial.print("Mensaje recibido: ");
  //  Serial.println(paquete_datos.word);
 
  if(!calibrating && !detecting)
  {
    Serial.print("RSSI: ");
    Serial.println(rssi_display);
    Serial.print("RSSI filtrado: ");
    Serial.println(rssi_filtered);
  }
}

/********************************** Funciones Filtro ************************************/

Filtro::Filtro() {
  x_est = -50;
  P_est = 100;
  varianzaR = 0.1;
  varianzaQ = 0.5;
}

Filtro::~Filtro() {
  // Destructor
}


float Filtro::filtroKalman(float nuevaMuestra) {
  float R = varianzaR;
  float Q = varianzaQ;
  
  float x_pred = x_est;
  float P_pred = P_est + Q;

  // Ganancia de Kalman
  float K = P_pred / (P_pred + R);

  // Actualización
  x_est = x_pred + K * (nuevaMuestra - x_pred);
  P_est = (1 - K) * P_pred;

  return x_est;
}

void Filtro::filtrado(float nuevaMuestra) {
  rssi_filtered = filtroKalman(nuevaMuestra);
}

void Filtro::set_varianzaR(float var) {
  varianzaR = var;
}

void Filtro::set_varianzaQ(float var) {
  varianzaQ = var;
}
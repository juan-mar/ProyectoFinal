#include "tag.h"

static struct_message_t paquete_datos;

static int rssi_display;
static float rssi_filtered;

static bool calibrating = false;
static unsigned long lastSampleTime = 0;


static long sumRSSI = 0;
static long sumSqRSSI = 0;
static int sampleCount = 0;


static Filtro filtro_kalman;

Receptor::Receptor() {
  threshold = 0;
  varianza = 0;
  //macAddr = 0;
  rssi_display = 0;

}

Receptor::~Receptor() {
  // Destructor
}

bool Receptor::calibracion() {
  calibrating = true;
  static unsigned long calibStart = millis();
  unsigned long now = millis();

  // Es momento de tomar una nueva muestra?
  if ((now - lastSampleTime) >= SAMPLE_INTERVAL) {
    Serial.println("calibrando...");
    lastSampleTime = now;

    int rssi_calib = this->getRSSI();
    Serial.print("Muestra RSSI: ");
    Serial.println(rssi_calib);

    sumRSSI += rssi_calib;
    sumSqRSSI += (long)rssi_calib * (long)rssi_calib; //suma el cuadrado de la muestra
    sampleCount++;
    return true;

  }
  else if ((now - calibStart) >= CAL_TIME) {
    Serial.println("Fin calibracion");
    
    calibrating = false;

    if (sampleCount > 0) {
      this -> threshold = (float)sumRSSI / sampleCount;
      this -> varianza  = ((float)sumSqRSSI / sampleCount)- (this -> threshold * this -> threshold);
      filtro_kalman.set_varianzaR(this -> varianza);
      Serial.print("Threshold:");
      Serial.println(this -> threshold);
      Serial.print("Varianza:");
      Serial.println(this -> varianza);
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
  rssi_display = rssi;
  if(!calibrating)
  {
    filtro_kalman.filtrado(rssi_display);
  }

  Serial.print("Type: ");
  Serial.println(type);

}

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&paquete_datos, incomingData, sizeof(paquete_datos));
//  Serial.print("Bytes received: ");
//  Serial.println(len);
//  Serial.print("Mensaje recibido: ");
//  Serial.println(paquete_datos.word);
  Serial.print("RSSI: ");
  Serial.println(rssi_display);
  Serial.print("RSSI filtrado: ");
  Serial.println(rssi_filtered);

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
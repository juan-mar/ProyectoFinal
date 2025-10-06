#include "tag.h"

static struct_message_t paquete_datos;

static int rssi_display;
static int rssi_filtered;

Receptor::Receptor() {
  threshold = 0;
  varianza = 0;
  calOK = false;
  calibrating = false;
  //macAddr = 0;
  rssi_display = 0;
  x_est = -60;
  P_est = 1;
}

Receptor::~Receptor() {
  // Destructor
}

void Receptor::calibracion() {
  calibrating = true;
  unsigned long calibStart = millis();
  unsigned long lastSampleTime = 0;

  long sumRSSI = 0;
  long sumSqRSSI = 0;
  int sampleCount = 0;
  unsigned long now = millis();

  // Es momento de tomar una nueva muestra?
  if (now - lastSampleTime >= SAMPLE_INTERVAL) {
    lastSampleTime = now;

    int rssi = Receptor::getRSSI();

    sumRSSI += rssi;
    sumSqRSSI += (long)rssi * (long)rssi; //suma el cuadrado de la muestra
    sampleCount++;

  }

if (now - calibStart >= CAL_TIME) {
    calibrating = false;

    if (sampleCount > 0) {
      this -> threshold = (float)sumRSSI / sampleCount;
      this -> varianza  = ((float)sumSqRSSI / sampleCount) - (this -> threshold * this -> threshold);

      // Guardar en NVS
      //prefs.begin("calib", false);
      //prefs.putFloat("mean", threshold);
      //prefs.putFloat("var", varianza);
      //prefs.end();


      //notifyCalibrationDone(); // acá le decís al TAG que prenda el LED
    }
  }
}

float Receptor::filtrado(float nuevaMuestra, float R, float Q) {
  float x_pred = x_est;
  float P_pred = P_est + Q;

  // Ganancia de Kalman
  float K = P_pred / (P_pred + R);

  // Actualización
  x_est = x_pred + K * (nuevaMuestra - x_pred);
  P_est = (1 - K) * P_pred;

  return x_est;
}

float Receptor::getRSSI() {
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
}

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&paquete_datos, incomingData, sizeof(paquete_datos));
//  Serial.print("Bytes received: ");
//  Serial.println(len);
//  Serial.print("Mensaje recibido: ");
//  Serial.println(paquete_datos.word);
  Serial.print("RSSI: ");
  Serial.println(rssi_display);
}
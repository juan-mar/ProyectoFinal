/*********
  Marce Ferra 2021
  Proyecto de emisor y receptor para tres entradas lógicas (para contacto seco) en emisor y tres salidas lógicas de estado en receptor
  Usando protocolo ESP NOW de Espressif
  MODULO RECEPTOR con intento de ver el RSSI del paquete recibido. Creo...
  
  Si esta información te resulta útil e interesante, invitame un cafecito!!!
  https://cafecito.app/marce_ferra

  Desde fuera de Argentina en:
  https://www.buymeacoffee.com/marceferra

  If you found this information useful and interesting, buy me a cafecito!!!
  https://www.buymeacoffee.com/marceferra
*********/


#include "esp_wifi.h"
#include <esp_now.h>
#include <WiFi.h>
#include <SPI.h>
#include <Wire.h>


//Para "pulso"
void pulso();
unsigned long previousMillis = 0;
const long interval = 3000;

int rssi_display;

// Estructura de datos para recibir
typedef struct struct_message {
  char word[12];
}struct_message;

struct_message paquete_datos;

// Estructuras para calcular los paquetes, el RSSI, etc
typedef struct {
  unsigned frame_ctrl: 16;
  unsigned duration_id: 16;
  uint8_t addr1[6]; /* receiver address */
  uint8_t addr2[6]; /* sender address */
  uint8_t addr3[6]; /* filtering address */
  unsigned sequence_ctrl: 16;
  uint8_t addr4[6]; /* optional */
} wifi_ieee80211_mac_hdr_t;

typedef struct {
  wifi_ieee80211_mac_hdr_t hdr;
  uint8_t payload[0]; /* network data ended with 4 bytes csum (CRC32) */
} wifi_ieee80211_packet_t;

//La callback que hace la magia
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

// callback function that will be executed when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&paquete_datos, incomingData, sizeof(paquete_datos));
  Serial.print("Bytes received: ");
  Serial.println(len);
  Serial.print("Mensaje recibido: ");
  Serial.println(paquete_datos.word);
  Serial.print("RSSI: ");
  Serial.println(rssi_display);
}

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);

  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }


  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  esp_now_register_recv_cb(OnDataRecv);

  //CB para el RSSI
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&promiscuous_rx_cb);

}

void loop() {
  pulso();
}

void pulso() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    digitalWrite(2, !digitalRead(2)); // toggle
  }
}

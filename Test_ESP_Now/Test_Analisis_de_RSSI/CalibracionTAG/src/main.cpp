#include <Arduino.h>
#include "tag.h"


// Variables globales
Receptor rx;




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
  // put your main code here, to run repeatedly:
  if (Serial.available() > 0) {        // si hay datos disponibles
    char ok = Serial.read();           // leer un carácter
    if (ok == 'c') {                   // si el carácter es 'c'
      rx.calibracion();              // llamar a la función calibracion
    }
  }
}

// put function definitions here:

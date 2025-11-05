#include <Arduino.h>
#include "tag.h"

#define THRES_TIME 3000

// Variables globales
Receptor rx;

bool calib = false;
bool detected = false;


void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);

    // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  Serial.println("Entrando en set up");

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  esp_now_register_recv_cb(OnDataRecv);

  //CB para el RSSI`
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&promiscuous_rx_cb);

  

}

void loop() {
  // put your main code here, to run repeatedly:
  if (Serial.available() > 0 || calib) { // si hay datos disponibles en el puerto serie
    char ok = Serial.read();           // leer un carácter
    if (ok == 'c' || calib) {          // si el carácter es 'c'
      calib = rx.calibracion();        // llamar a la función calibracion
    }
    else if (ok == 'd' || detected) {               // si el carácter es 'd' o ya se detecto la señal
      static unsigned long calibStart = millis();
      unsigned long now = millis();
      detected = rx.detect_thres(); // llamar a la función detect_thres
      if (detected && now - calibStart < THRES_TIME) {
        Serial.println("Dentro del umbral");
      } 
      else if(detected && now - calibStart >= THRES_TIME) {
        Serial.println("Señal detectada!");
      }
      else {
        Serial.println("Fuera del umbral");
      }
  }
}
}
// put function definitions here:

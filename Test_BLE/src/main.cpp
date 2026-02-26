#include <Arduino.h>
#include <WiFi.h>
#include "rx.h"


/******************* CÓDIGO PRINCIPAL ********************/
Receptor scanner(MAC_ADDR);

void setup() {
  Serial.begin(115200);
  scanner.init();                       //Inicia los callbacks
  pinMode(2, OUTPUT);
  Serial.println("Scanner Continuo Iniciado.");
}

void loop() {
  // Ciclo infinito que hace la limpieza de datos
  scanner.scan(0);
}



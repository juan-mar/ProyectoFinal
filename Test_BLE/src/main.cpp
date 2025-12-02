#include <Arduino.h>

// Pines para el HM-10 (Serial2 en ESP32)
#define RXD2 16
#define TXD2 17

void setup() {
  // Comunicación con la PC
  Serial.begin(115200);
  
  // Comunicación con el HM-10 (Por defecto suelen venir a 9600 baudios)
  // Si no te responde, prueba cambiar este 9600 por 115200
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  
  Serial.println("--- MODO PUENTE ACTIVADO ---");
  Serial.println("Escribe 'AT' y presiona Enter para probar.");
}

void loop() {
  // Si el HM-10 dice algo, imprímelo en la PC
  if (Serial2.available()) {
    Serial.write(Serial2.read());
  }
  
  // Si tú escribes algo en la PC, mándalo al HM-10
  if (Serial.available()) {
    Serial2.write(Serial.read());
  }
}
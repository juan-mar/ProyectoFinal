#include <Arduino.h>

/*
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

*/
  
#include "rx.h"


String MI_MAC = "9C:1D:58:95:7B:9C";
Receptor scanner(MI_MAC);

// Variables para TU lógica de promedio
const int UMBRAL = -60;
int promedio = -100;

void setup() {
  Serial.begin(115200);
  scanner.init();
  Serial.println("Scanner Continuo Iniciado.");
}

void loop() {
  // 1. MANTENIMIENTO (Obligatorio llamarlo para que el ciclo infinito funcione)
  scanner.loop();

  // 2. USO DE DATOS
  // Leemos la variable global directamente
  int lecturaRaw = scanner.rssiActual;
  
  // Opcional: Seguridad por si se apaga el dispositivo
  if (millis() - scanner.ultimaActualizacion > 3500) {
      lecturaRaw = -100; // Si no hay datos en 3.5s, asumimos lejos
      Serial.println("--- Perdió señal ---");
  }

  // Aquí haces tu promedio o lógica
  Serial.print("RSSI en tiempo real: ");
  Serial.println(lecturaRaw);

  // Pequeño delay para no saturar el monitor serie (no afecta al bluetooth)
  //delay(100); 
}
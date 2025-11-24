#include <Arduino.h>
#include "rx.h"

// --- CONFIGURACIÓN ---
const int PIN_RF = 18;         // Pin conectado al Data Out del RF433
const int PIN_LANZADOR = 25;   // Pin para activar el motor/premio
const long TIEMPO_META = 5000; // 5 segundos

// Instanciamos el objeto
ReceptorRF miReceptor(PIN_RF, TIEMPO_META);

void setup() {
  Serial.begin(115200);
  
  pinMode(PIN_LANZADOR, OUTPUT);
  digitalWrite(PIN_LANZADOR, LOW); // Apagado por defecto

  // Inicializamos el receptor
  miReceptor.begin();
  
  Serial.println("Sistema Listo. Esperando señal del collar...");
}

void loop() {

  // actualizar() revisa si esta enlazado el tag o no
  if (miReceptor.actualizar()) {
    
    Serial.println("Enlazado");
    
    // Acción: Lanzar pelota
    //lanzarPremio();

    // Reiniciar para el siguiente intento
    //miReceptor.reset();
    //Serial.println("Reiniciando... Esperando nueva señal.");
  }
}

void lanzarPremio() {
  Serial.println("Activando lanzador");
  digitalWrite(PIN_LANZADOR, HIGH);
  delay(1000); // Mantenemos activo el lanzador 1 segundo
  digitalWrite(PIN_LANZADOR, LOW);
}
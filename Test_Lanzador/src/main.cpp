#include <Arduino.h>

// --- DEFINICIÓN DE PINES ---
// Asegúrate de cambiar este número al GPIO donde conectaste el Gate del 2N7002
uint8_t LOAD_SW_PIN = 4;   // Pin para Habilitar/Deshabilitar la alimentación (Switch Load)

uint8_t TRIG_PIN = 5;      // Pin de disparo del solenoide (Gate del MOSFET Low-Side)
uint8_t PULSE_PIN = 14;    // (Opcional o auxiliar según tu esquema anterior)
unsigned long PULSE_DURATION_MS = 120; // Duración del pulso en ms

// --- FUNCIONES ---

void sendPulse() {
  // Solo avisamos, pero el disparo físico depende de si el Load Switch está prendido
  Serial.print(F("Disparando... ")); 
  digitalWrite(PULSE_PIN, HIGH);
  digitalWrite(TRIG_PIN, HIGH);
  delay(PULSE_DURATION_MS);
  digitalWrite(PULSE_PIN, LOW);
  digitalWrite(TRIG_PIN, LOW);
  Serial.println(F("Fin del pulso."));
}

// Para mosfet invertido (si usaras lógica negativa en el disparo)
void sendPulseInv() {
  digitalWrite(PULSE_PIN, LOW);
  digitalWrite(TRIG_PIN, LOW);
  delay(PULSE_DURATION_MS);
  digitalWrite(PULSE_PIN, HIGH);
  digitalWrite(TRIG_PIN, HIGH);
}

void setup() {
  Serial.begin(115200);

  // 1. Configurar Pin del Switch Load (Alimentación)
  pinMode(LOAD_SW_PIN, OUTPUT);
  // IMPORTANTE: Iniciamos en LOW para que el sistema arranque APAGADO por seguridad.
  // Recordatorio: GPIO LOW -> 2N7002 OFF -> PMOS Gate Pull-Up -> PMOS OFF.
  digitalWrite(LOAD_SW_PIN, LOW); 

  // 2. Configurar Pines de Disparo
  pinMode(PULSE_PIN, OUTPUT);
  digitalWrite(PULSE_PIN, LOW);

  pinMode(TRIG_PIN, OUTPUT);
  digitalWrite(TRIG_PIN, LOW);

  // Esperar a que el puerto serie conecte (útil para ESP32 nativos)
  while (!Serial) {
    delay(10);
  }

  Serial.println(F("--- SISTEMA DE CONTROL DE TESIS ---"));
  Serial.println(F("'H' -> Habilitar Energía (High)"));
  Serial.println(F("'L' -> Deshabilitar Energía (Low)"));
  Serial.println(F("'P' -> Disparar Solenoide (Pulse)"));
  Serial.println(F("-----------------------------------"));
}

void loop() {
  if (!Serial.available()) {
    return;
  }

  // Leemos el caracter y lo convertimos a mayúscula por si acaso
  char incoming =  toupper(static_cast<char>(Serial.read()));

  // Ignoramos saltos de línea (\n) o retornos de carro (\r) del monitor serie
  if (incoming == '\n' || incoming == '\r') return;

  if (incoming == 'P') {
    sendPulse();
  } 
  else if (incoming == 'H') {
    digitalWrite(LOAD_SW_PIN, HIGH);
    Serial.println(F(">> COMANDO: Switch Load HABILITADO (ON)"));
  } 
  else if (incoming == 'L') {
    digitalWrite(LOAD_SW_PIN, LOW);
    Serial.println(F(">> COMANDO: Switch Load DESHABILITADO (OFF)"));
  }
}
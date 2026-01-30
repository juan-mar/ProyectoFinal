#include <Arduino.h>

// --- DEFINICIÓN DE PINES ---

// 1. Etapa de 12V (Switch Load 1)
const uint8_t PIN_ENABLE_12V = 4;   // Pin que activa la batería hacia la Boost de 12V

// 2. Etapa de 80V (Switch Load 2 - NUEVO)
// IMPORTANTE: Conecta este pin al Gate del 2N7002 que controla la entrada de 80V
const uint8_t PIN_ENABLE_80V = 16;  // <--- CAMBIA ESTE NUMERO SI USAS OTRO PIN

// 3. Disparo (Solenoides)
const uint8_t TRIG_PIN = 5;       // Pin de disparo del solenoide
const uint8_t PULSE_PIN = 14;     // Auxiliar (LED o señal)

// --- CONFIGURACIÓN ---
unsigned long PULSE_DURATION_MS = 120; // Duración del pulso en ms
const int TIEMPO_ESPERA_ARRANQUE = 100; // Tiempo de espera entre 12V y 80V (2 segundos)

// --- FUNCIONES ---

void sendPulse() {
  Serial.print(F("Disparando Solenoide... ")); 
  digitalWrite(PULSE_PIN, HIGH);
  digitalWrite(TRIG_PIN, HIGH);
  
  delay(PULSE_DURATION_MS); // Tiempo que el solenoide está activo
  
  digitalWrite(PULSE_PIN, LOW);
  digitalWrite(TRIG_PIN, LOW);
  Serial.println(F("Fin del pulso."));
}

void encenderSistemaSecuencial() {
  Serial.println(F(">> INICIANDO SECUENCIA DE ENCENDIDO..."));
  
  // 1. Encender Etapa 1 (12V)
  digitalWrite(PIN_ENABLE_12V, HIGH);
  Serial.println(F("   1. Boost 12V: ENCENDIDA."));
  Serial.println(F("   ... Esperando estabilizacion (2 seg) ..."));

  // 2. Espera de seguridad (para que carguen los capacitores de 12V)
  delay(TIEMPO_ESPERA_ARRANQUE);

  // 3. Encender Etapa 2 (80V)
  digitalWrite(PIN_ENABLE_80V, HIGH);
  Serial.println(F("   2. Boost 80V: ENCENDIDA (Sistema Listo)."));
}

void apagarSistema() {
  // Apagamos todo junto (o primero la alta tensión por seguridad)
  digitalWrite(PIN_ENABLE_80V, LOW);
  digitalWrite(PIN_ENABLE_12V, LOW);
  Serial.println(F(">> SISTEMA APAGADO COMPLETAMENTE."));
}

void setup() {
  Serial.begin(115200);

  // Configurar Pines
  pinMode(PIN_ENABLE_12V, OUTPUT);
  pinMode(PIN_ENABLE_80V, OUTPUT);
  pinMode(PULSE_PIN, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);

  // ESTADO INICIAL: Todo Apagado
  digitalWrite(PIN_ENABLE_12V, LOW);
  digitalWrite(PIN_ENABLE_80V, LOW);
  digitalWrite(PULSE_PIN, LOW);
  digitalWrite(TRIG_PIN, LOW);

  // Esperar puerto serie
  while (!Serial) { delay(10); }

  Serial.println(F("--- CONTROL DE LANZADOR (SECUENCIAL) ---"));
  Serial.println(F("'H' -> Habilitar Energía (Secuencia 12V -> 2s -> 80V)"));
  Serial.println(F("'L' -> Apagar Todo"));
  Serial.println(F("'P' -> Disparar Solenoide"));
  Serial.println(F("----------------------------------------"));
}

void loop() {
  if (!Serial.available()) return;

  char incoming = toupper(static_cast<char>(Serial.read()));
  
  // Limpiar saltos de línea
  if (incoming == '\n' || incoming == '\r') return;

  switch (incoming) {
    case 'P':
      sendPulse();
      break;
      
    case 'H':
      encenderSistemaSecuencial();
      break;
      
    case 'L':
      apagarSistema();
      break;
  }
}
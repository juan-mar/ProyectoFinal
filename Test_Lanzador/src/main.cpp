#include <Arduino.h>
#include <rom/rtc.h> // Necesario para acceder a los registros de reset
// --- DEFINICIÓN DE PINES ---

// 1. CONTROL DE ENCENDIDO (Interruptor General)
// Conectar a GND para ENCENDER el sistema.
const uint8_t PIN_SWITCH_ON = 22;    

// 2. CONTROL DE DISPARO (Pulsador)
// Conectar a GND para DISPARAR (pull-up interno).
// Cambia este número por el pin que decidas usar.
const uint8_t PIN_BUTTON_TRIGGER = 21; 

// 3. SALIDAS DE POTENCIA
const uint8_t PIN_ENABLE_12V = 33;   // Etapa 12V
const uint8_t PIN_ENABLE_80V = 32;  // Etapa 80V

// 4. SALIDAS DE ACTUADORES
const uint8_t TRIG_PIN = 4;       // Gate del MOSFET del Solenoide
const uint8_t PULSE_PIN = 5;     // LED auxiliar o buzzer

const int PIN_ERROR_INDICATOR = 2; // Usualmente el LED azul integrado
// --- CONFIGURACIÓN ---
unsigned long PULSE_DURATION_MS = 120;   // Tiempo que el solenoide se activa
const int TIEMPO_ESPERA_ARRANQUE = 100; // Tiempo entre 12V y 80V

// --- VARIABLES DE ESTADO ---
bool sistemaActivo = false;      // ¿Está cargado el sistema?
bool lastTriggerState = LOW;    // Para detectar el flanco del botón de disparo

// --- FUNCIONES ---

void sendPulse() {
  // Doble chequeo de seguridad
  if (!sistemaActivo) return;

  digitalWrite(PULSE_PIN, HIGH);
  digitalWrite(TRIG_PIN, HIGH);
  
  delay(PULSE_DURATION_MS); 
  
  digitalWrite(PULSE_PIN, LOW);
  digitalWrite(TRIG_PIN, LOW);
}

void encenderSistemaSecuencial() {
  if (sistemaActivo) return;

  // 1. Encender Etapa 1 (12V)
  digitalWrite(PIN_ENABLE_12V, HIGH);
  
  // 2. Espera de seguridad (Carga de capacitores)
  delay(TIEMPO_ESPERA_ARRANQUE);

  // 3. Encender Etapa 2 (80V)
  digitalWrite(PIN_ENABLE_80V, HIGH);

  sistemaActivo = true; 
}

void apagarSistema() {
  if (!sistemaActivo) return;

  // Apagar todo inmediatamente
  digitalWrite(PIN_ENABLE_80V, LOW);
  digitalWrite(PIN_ENABLE_12V, LOW);
  digitalWrite(TRIG_PIN, LOW);
  digitalWrite(PULSE_PIN, LOW);

  sistemaActivo = false; 
}

void setup() {
  // (Serial eliminado o dejado solo para debug opcional, no afecta)
  // Serial.begin(115200);

  pinMode(PIN_ERROR_INDICATOR, OUTPUT);
    digitalWrite(PIN_ERROR_INDICATOR, LOW);

    // 1. Preguntar al ESP32: "¿Por qué te reiniciaste?"
    esp_reset_reason_t reason = esp_reset_reason();

    // 2. Verificar si fue por Brownout
    if (reason == ESP_RST_BROWNOUT) {
        
        // --- OPCIÓN A: Patrón de parpadeo rápido (Alarma Visual) ---
        // Parpadear frenéticamente por 5 segundos para avisar
        for (int i = 0; i < 25; i++) {
            digitalWrite(PIN_ERROR_INDICATOR, HIGH);
            delay(100);
            digitalWrite(PIN_ERROR_INDICATOR, LOW);
            delay(100);
        }
        
        // --- OPCIÓN B: Dejar el pin en HIGH (Para medir con multímetro) ---
        // Si prefieres medir voltaje, descomenta esto y comenta el bucle for
        // digitalWrite(PIN_ERROR_INDICATOR, HIGH);
        // delay(5000); // Mantener 5 segundos en ALTO para que te de tiempo a medir
        // digitalWrite(PIN_ERROR_INDICATOR, LOW);
    }

  // --- CONFIGURAR SALIDAS ---
  pinMode(PIN_ENABLE_12V, OUTPUT);
  pinMode(PIN_ENABLE_80V, OUTPUT);
  pinMode(PULSE_PIN, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);

  // --- CONFIGURAR ENTRADA (PULLUP INTERNO) ---
  // Lee HIGH (1) cuando está al aire.
  // Lee LOW (0) cuando se conectan a GND.
  pinMode(PIN_SWITCH_ON, INPUT_PULLUP);

  // --- CONFIGURAR ENTRADA (PULLUP INTERNO) ---
  // Lee HIGH (1) cuando está al aire.
  // Lee LOW (0) cuando se conectan a GND.
  pinMode(PIN_BUTTON_TRIGGER, INPUT_PULLUP);

  // Estado Inicial Seguro
  digitalWrite(PIN_ENABLE_12V, LOW);
  digitalWrite(PIN_ENABLE_80V, LOW);
  digitalWrite(TRIG_PIN, LOW);
  digitalWrite(PULSE_PIN, LOW);
}

void loop() {
  // ---------------------------------------------------------
  // 1. GESTIÓN DE ENCENDIDO / APAGADO (Interruptor sostenido)
  // ---------------------------------------------------------
  bool switchState = digitalRead(PIN_SWITCH_ON);

  if (switchState == LOW) {
    // El interruptor está puesto a GND -> ENCENDER
    if (!sistemaActivo) {
      encenderSistemaSecuencial();
    }
  } else {
    // El interruptor está abierto -> APAGAR
    if (sistemaActivo) {
      apagarSistema();
    }
  }

  // ---------------------------------------------------------
  // 2. GESTIÓN DE DISPARO (Pulsador momentáneo)
  // ---------------------------------------------------------
  // Solo leemos el disparo si el sistema está activo (cargado)
  if (sistemaActivo) {
    bool currentTriggerState = digitalRead(PIN_BUTTON_TRIGGER);

    // Detectamos FLANCO DESCENDENTE (De HIGH a LOW)
    // Esto significa que el botón se acaba de presionar en este instante
    if (lastTriggerState == HIGH && currentTriggerState == LOW) {
      sendPulse();
      // Pequeño delay anti-rebote para el disparo
      delay(200); 
    }
    
    // Guardamos el estado para comparar en la siguiente vuelta
    lastTriggerState = currentTriggerState;
  }

  // Pequeño retardo para estabilidad general del bucle
  delay(20); 
}
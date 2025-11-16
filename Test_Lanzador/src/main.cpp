#include <Arduino.h>

uint8_t TRIG_PIN = 5;                 // GPIO used to drive the pulse
uint8_t PULSE_PIN = 14;
unsigned long PULSE_DURATION_MS = 120; // Pulse length; adjust as needed

void sendPulse() {
  digitalWrite(PULSE_PIN, HIGH);
  digitalWrite(TRIG_PIN, HIGH);
  delay(PULSE_DURATION_MS);
  digitalWrite(PULSE_PIN, LOW);
  digitalWrite(TRIG_PIN, LOW);
}

//Para mosfet
void sendPulseInv() {
  digitalWrite(PULSE_PIN, LOW);
  digitalWrite(TRIG_PIN, LOW);
  delay(PULSE_DURATION_MS);
  digitalWrite(PULSE_PIN, HIGH);
  digitalWrite(TRIG_PIN, HIGH);
}

void setup() {
  Serial.begin(115200);
  pinMode(PULSE_PIN, OUTPUT);
  digitalWrite(PULSE_PIN, LOW);

  pinMode(TRIG_PIN, OUTPUT);
  digitalWrite(TRIG_PIN, LOW);

  while (!Serial) {
  }

  Serial.println(F("Listo: envie 'P' por serial para generar un pulso"));
}

void loop() {
  if (!Serial.available()) {
    return;
  }

  char incoming = static_cast<char>(Serial.read());
  if (incoming == 'P') {
    sendPulseInv();
    Serial.println(F("Pulso enviado"));
  }
}

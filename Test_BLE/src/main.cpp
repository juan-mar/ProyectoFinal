#include <Arduino.h>
#include "rx.h"


/********************* DEFINICIONES *********************/
#define THRES_TIME 3000
#define MAC_ADDR "9C:1D:58:95:7B:9C"


/****************** VARIABLES GLOBALES ******************/
const int umbral = -60;
int promedio = -100;

bool calib = false;
bool detected = false;
bool flag = false;

unsigned long calibStart;


/******************* CÓDIGO PRINCIPAL ********************/
Receptor scanner(MAC_ADDR);

void setup() {
  Serial.begin(115200);
  scanner.init(); //Inicia los callbacks
  Serial.println("Scanner Continuo Iniciado.");
}

void loop() {
  // Ciclo infinito que hace la limpieza de datos
  scanner.loop();

  // Leemos la variable global directamente
  int lecturaRaw = scanner.getRSSI();
  
  // Seguridad por si se apaga el dispositivo
  if (millis() - scanner.getUltimaActualizacion() > 3500) {
      lecturaRaw = -100; // Si no hay datos en 3.5s, asumimos lejos
      Serial.println("--- Perdió señal ---");
  }

  if (scanner.isNewMsg() || calib || flag) { // si hay datos disponibles en el puerto serie
    char ok = Serial.read();           // leer un carácter
    if (ok == 'c' || calib) {          // si el carácter es 'c'
      calib = scanner.calibracion();        // llamar a la función calibracion
    }
    else if (ok == 'd' || flag) {               // si el carácter es 'd' o ya se detecto la señal
      if(!flag) {
      calibStart = millis();
      }
      flag = true;
      unsigned long now = millis();
      detected = scanner.detect_thres(); // llamar a la función detect_thres
      if (detected && now - calibStart < THRES_TIME) {
        Serial.println("Dentro del umbral");
      } 
      else if(detected && now - calibStart >= THRES_TIME) {
        Serial.println("Señal detectada!");
        flag = false;
      }
      else {
        Serial.println("Fuera del umbral");
        calibStart = millis();
      }
    }
  }

  // Pequeño delay para no saturar el monitor serie (no afecta al bluetooth)
  //delay(100); 
}
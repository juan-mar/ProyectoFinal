#include <Arduino.h>
#include "tag.h"


// Variables globales
Receptor rx;

bool calib = false;


void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);


}

void loop() {
  // put your main code here, to run repeatedly:
  if (Serial.available() > 0 || calib) { // si hay datos disponibles en el puerto serie
    char ok = Serial.read();           // leer un carácter
    if (ok == 'c' || calib) {          // si el carácter es 'c'
      calib = rx.calibracion();                // llamar a la función calibracion
    }
  }
}

// put function definitions here:

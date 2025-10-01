#include <Arduino.h>
#include "tag.h"


// Variables globales
tag tag1;




void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200); // inicializar comunicación serie


}

void loop() {
  // put your main code here, to run repeatedly:
  if (Serial.available() > 0) {        // si hay datos disponibles
    char ok = Serial.read();           // leer un carácter
    if (ok == 'c') {                   // si el carácter es 'c'
      tag1.calibracion();              // llamar a la función calibracion
    }
  }
}

// put function definitions here:

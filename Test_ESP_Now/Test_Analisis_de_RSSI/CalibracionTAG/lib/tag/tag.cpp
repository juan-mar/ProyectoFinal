#include "tag.h"

tag::tag() {
  threshold = 0;
  varianza = 0;
  calOK = false;
  calibrating = false;
}

tag::~tag() {
  // Destructor
}

void tag::calibracion() {
  calibrating = true;
  unsigned long calibStart = millis();
  unsigned long lastSampleTime = 0;

  long sumRSSI = 0;
  long sumSqRSSI = 0;
  int sampleCount = 0;
  unsigned long now = millis();

  // Es momento de tomar una nueva muestra?
  if (now - lastSampleTime >= SAMPLE_INTERVAL) {
    lastSampleTime = now;

    int rssi = tag::getRSSI(); 

    sumRSSI += rssi;
    sumSqRSSI += (long)rssi * (long)rssi; //suma el cuadrado de la muestra
    sampleCount++;

  }

if (now - calibStart >= CAL_TIME) {
    calibrating = false;

    if (sampleCount > 0) {
      threshold = (float)sumRSSI / sampleCount;
      varianza  = ((float)sumSqRSSI / sampleCount) - (threshold * threshold);

      // Guardar en NVS
      //prefs.begin("calib", false);
      //prefs.putFloat("mean", threshold);
      //prefs.putFloat("var", varianza);
      //prefs.end();


      //notifyCalibrationDone(); // acá le decís al TAG que prenda el LED
    }
  }
}

float tag::getRSSI() {
  // Simulación de lectura de RSSI
  return -60.0; // valor simulado
}

float tag::getThreshold() {
  return threshold;
}

float tag::getVarianza() {
  return varianza;
}
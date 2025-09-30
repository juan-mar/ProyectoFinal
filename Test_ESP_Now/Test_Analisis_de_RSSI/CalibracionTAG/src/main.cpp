#include <Arduino.h>
#include <Preferences.h>

// put function declarations here:
void calibracion();

// Variables globales
Preferences preferences;
float threshold = 0.0; // Umbral de calibración
float varianza = 0.0; // Varianza de calibración
int numMuestras = 100; // Número de muestras para calcular el umbral y la varianza
bool calOK = 0; // Variable para indicar si la calibración fue exitosa
int calTime = 5000; // Tiempo de espera para la calibración en milisegundos

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200); // inicializar comunicación serie

}

void loop() {
  // put your main code here, to run repeatedly:
  if (Serial.available() > 0) {       // si hay datos disponibles
    char ok = Serial.read();           // leer un carácter
    if (ok == 'c') {                   // si el carácter es 'c'
      calibracion();   // llamar a la función calibracion
    }
  }
}

// put function definitions here:
void calibracion(){
  // Código de calibración aquí
  unsigned long start = millis();
  int count = 0;
  long sumRSSI = 0;

  while (millis() - start < calTime) {
    int rssi = 0;
    //rssi = getRSSI(); // <-- tu función para leer RSSI del TAG
    sumRSSI += rssi;
    count++;
    delay(100); // leer cada 100ms
  }

  threshold = sumRSSI / count;  //promedio de RSSI
  calOK = true;
  // Avisar al TAG o prender LED local
  //notifyCalibrationDone();


  // Guardar en NVS
  //prefs.begin("calib", false);
  //prefs.putInt("threshold", threshold);
  //prefs.end();


}
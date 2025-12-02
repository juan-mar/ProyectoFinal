#include <Arduino.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// --- TU MAC ADDRESS ---
// Ya está escrita con el formato correcto
String TARGET_MAC = "9C:1D:58:95:7B:9C"; 

// --- CONFIGURACIÓN DE DISTANCIA ---
// Calibra esto: 
// -60 suele ser ~1 metro. 
// Si quieres que detecte más lejos, pon -70 o -80.
// Si quieres que detecte más cerca (40cm), pon -50.
int UMBRAL_RSSI = -65; 

int SCAN_TIME = 1; // Escaneo rápido (1 segundo)
BLEScan* pBLEScan;

// Variables para el promedio (Suavizado)
int ultimosRSSI[5]; // Guardamos las últimas 5 lecturas
int indiceLectura = 0;
bool bufferLleno = false;

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      
      String macLeida = advertisedDevice.getAddress().toString().c_str();
      macLeida.toUpperCase(); // Convertimos a mayúsculas para asegurar coincidencia

      // Comparamos con TU módulo
      if (macLeida == TARGET_MAC) {
          
          int rssiActual = advertisedDevice.getRSSI();
          
          // --- LÓGICA DE PROMEDIO ---
          // Guardamos el valor en el array
          ultimosRSSI[indiceLectura] = rssiActual;
          indiceLectura++;
          if (indiceLectura >= 5) {
            indiceLectura = 0;
            bufferLleno = true;
          }

          // Solo calculamos si ya tenemos 5 lecturas para ser precisos
          if (bufferLleno) {
            long suma = 0;
            for (int i = 0; i < 5; i++) {
              suma += ultimosRSSI[i];
            }
            int rssiPromedio = suma / 5;

            Serial.print("MAC: ");
            Serial.print(macLeida);
            Serial.print(" | RSSI Real: ");
            Serial.print(rssiActual);
            Serial.print(" | PROMEDIO: ");
            Serial.print(rssiPromedio);

            // --- DECISIÓN FINAL ---
            if (rssiPromedio > UMBRAL_RSSI) {
               Serial.println("  >>> ESTÁS DENTRO DEL RANGO (1m) <<<");
               // AQUÍ PONES TU CÓDIGO DE ACCIÓN (Ej: encender LED)
            } else {
               Serial.println("  --- Lejos ---");
            }
          } else {
             Serial.print("Calibrando... (");
             Serial.print(indiceLectura);
             Serial.println("/5)");
          }
      }
    }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando Radar de Proximidad...");

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); 
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
}

void loop() {
  // Escaneo continuo
  pBLEScan->start(SCAN_TIME, false);
  pBLEScan->clearResults();
}
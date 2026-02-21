#include <Arduino.h>
#include <WiFi.h>
#include "rx.h"


/********************* DEFINICIONES *********************/
#define THRES_TIME 3000
#define MAC_ADDR "9C:1D:58:95:7B:9C"    //mac address del emisor, la señal que queremos escuchar


/**************** CREDENCIALES WI-FI ********************/
// <-- 2. VARIABLES DE WIFI AGREGADAS
const char* ssid = "Fibertel WiFi373 2.4GHz"; 
const char* password = "00424498516";

/****************** VARIABLES GLOBALES ******************/
const int umbral = -60;
int promedio = -100;

bool calib = false;
bool detected = false;
bool flag = false;

unsigned long calibStart;


/******************* CÓDIGO PRINCIPAL ********************/
Receptor scanner(MAC_ADDR);

bool conectarWiFi(const char* ssid, const char* password) {
    Serial.println();
    Serial.print("Conectando a la red Wi-Fi: ");
    Serial.println(ssid);

    // Configuramos el ESP32 en modo "Estación" (cliente), no como un router (Punto de acceso)
    WiFi.mode(WIFI_STA);
    
    // Desconectamos cualquier conexión previa por si acaso
    WiFi.disconnect();
    delay(100);

    // Iniciamos la conexión
    WiFi.begin(ssid, password);

    // Esperamos a que se conecte, con un límite de tiempo (ej. 10 segundos)
    int intentos = 0;
    const int MAX_INTENTOS = 20; // 20 intentos de 500ms = 10 segundos

    while (WiFi.status() != WL_CONNECTED && intentos < MAX_INTENTOS) {
        delay(500);
        Serial.print(".");
        intentos++;
    }

    Serial.println(); // Salto de línea después de los puntitos

    // Verificamos el resultado final
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("¡Conexión Wi-Fi exitosa!");
        Serial.print("Dirección IP asignada: ");
        Serial.println(WiFi.localIP());
        return true;
    } else {
        Serial.println("Error: No se pudo conectar al Wi-Fi. Tiempo de espera agotado.");
        return false;
    }
}

void setup() {
  Serial.begin(115200);
  scanner.init();                       //Inicia los callbacks
  pinMode(2, OUTPUT);
  Serial.println("Scanner Continuo Iniciado.");
}

void loop() {
  // Ciclo infinito que hace la limpieza de datos
  scanner.loop();

  // Leemos la variable global directamente
  int lecturaRaw = scanner.getRSSI();
  
  // Seguridad por si se apaga el dispositivo
  //if (millis() - scanner.getUltimaActualizacion() > 3500) {
  //    lecturaRaw = -100; // Si no hay datos en 3.5s, asumimos lejos
  //    Serial.println("--- Perdió señal ---");
  //}

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
        digitalWrite(2, HIGH); // Enciende el LED
      } 
      else if(detected && now - calibStart >= THRES_TIME) {
        Serial.println("Señal detectada!");
        digitalWrite(2, LOW); // Apaga el LED
        flag = false;
      }
      else {
        digitalWrite(2, LOW); // Apaga el LED
        calibStart = millis();
      }
    }
    else if(ok == 's') {
      Serial.println("Comando 's' recibido. Apagando BLE...");
      
      // 1. Apagamos todo lo relacionado al Bluetooth
      scanner.stop(); 
      flag = false;  // Por si estaba detectando, lo frenamos
      calib = false; // Por si estaba calibrando, lo frenamos
      
      // 2. Encendemos el Wi-Fi
      if (conectarWiFi(ssid, password)) {
          Serial.println("¡Transición exitosa! El ESP32 ya tiene Internet.");
          // >>> AQUÍ VA TU CÓDIGO PARA ENVIAR DATOS A LA NUBE <<<
      } else {
          Serial.println("No se pudo conectar al Wi-Fi.");
      }
    }
  }

}



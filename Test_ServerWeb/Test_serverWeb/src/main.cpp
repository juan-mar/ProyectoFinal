#include <Arduino.h>
#include "WebServerManager.h"

WebServerManager* webServer = nullptr;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n--- Test Server Web ---");

    // Crear e iniciar
    webServer = new WebServerManager();
    webServer->begin();

    Serial.println("Instrucciones:");
    Serial.println("1. Conecta tu celular al WiFi 'Lanzador_Config'.");
    Serial.println("2. Debería abrirse el navegador (Portal Cautivo).");
    Serial.println("3. Si no, ve a http://192.168.4.1");
}

void loop() {
    // ¡IMPORTANTE! Mantener el DNS vivo
    if (webServer != nullptr) {
        webServer->update();
    }
    
    // Pequeño delay para no saturar CPU (en el proyecto final esto va en una tarea)
    delay(10); 
}
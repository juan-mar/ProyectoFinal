#ifndef WEB_SERVER_MANAGER_H
#define WEB_SERVER_MANAGER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

class WebServerManager {
public:
    WebServerManager();
    
    // Inicia WiFi AP, DNS y Servidor Web
    void begin();
    
    // Detiene todo
    void stop();

    // Mantiene el DNS vivo (llamar en loop o update de estado)
    void update();

private:
    AsyncWebServer server;
    DNSServer dnsServer;

    const char* AP_SSID = "Lanzador_Config";
    // const char* AP_PASS = "12345678"; // Descomentar si se quiere pass
    const int DNS_PORT = 53;

    // Métodos internos
    void setupRoutes();
    String getDogsJson(); // Mockup para test
};

#endif
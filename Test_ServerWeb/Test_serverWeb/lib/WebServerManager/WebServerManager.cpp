#include "WebServerManager.h"
#include "config.h" // Importante para LOG_PRINTLN

WebServerManager::WebServerManager() : server(80) {
    // Constructor
}

void WebServerManager::begin() {
    LOG_PRINTLN("WS: Iniciando WebServerManager...");

    // 1. Montar LittleFS
    if (!LittleFS.begin()) {
        LOG_PRINTLN("WS: Error montando LittleFS! Formateando...");
        LittleFS.format();
        LittleFS.begin();
    }

    // 2. Iniciar WiFi AP
    WiFi.softAP(AP_SSID);
    IPAddress myIP = WiFi.softAPIP();
    
    // Usamos LOG_PRINT para construir la línea con la IP
    LOG_PRINT("WS: AP Iniciado. IP: ");
    LOG_PRINTLN(myIP);

    // 3. Iniciar DNS (Portal Cautivo)
    dnsServer.start(DNS_PORT, "*", myIP);

    // 4. Configurar Rutas
    setupRoutes();

    // 5. Arrancar Servidor
    server.begin();
    LOG_PRINTLN("WS: Servidor HTTP iniciado.");
}

void WebServerManager::stop() {
    dnsServer.stop();
    server.end();
    WiFi.softAPdisconnect(true);
    LOG_PRINTLN("WS: Servidor detenido.");
}

void WebServerManager::update() {
    dnsServer.processNextRequest();
}

// --- CONFIGURACIÓN DE RUTAS ---

void WebServerManager::setupRoutes() {
    
    // A. API: Obtener lista de perros (GET)
    server.on("/api/dogs", HTTP_GET, [this](AsyncWebServerRequest *request){
        String json = this->getDogsJson(); 
        request->send(200, "application/json", json);
        LOG_PRINTLN("WS: API GET /api/dogs servido.");
    });

    // B. API: Obtener estado (GET)
    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
        StaticJsonDocument<200> doc;
        doc["battery"] = 85; // Mockup
        doc["pending_sessions"] = 3; // Mockup
        
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    // C. API: Empezar Entrenamiento (POST)
    server.on("/api/start", HTTP_POST, 
        [](AsyncWebServerRequest *request){
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        },
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            
            // Reconstruir body
            String body = "";
            for(size_t i=0; i<len; i++) body += (char)data[i];
            
            LOG_PRINTLN("WS: API POST /api/start recibido:");
            LOG_PRINTLN(body);

            // Parsear JSON
            StaticJsonDocument<512> doc;
            DeserializationError error = deserializeJson(doc, body);

            if (!error) {
                const char* dog = doc["dog_code"];
                const char* mode = doc["mode"] | "manual"; // Default si no viene                // Usamos LOG_PRINTF para formateo limpio
                LOG_PRINTF("--> ACCIÓN: Iniciar %s con %s\n", mode, dog);
                
                // AQUÍ ENCOLARÍAS EL EVENTO A LA FSM EN EL FUTURO
            } else {
                LOG_PRINTLN("--> Error parseando JSON");
            }
    });

    // D. RUTAS ESTÁTICAS
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    // E. NOT FOUND / PORTAL CAUTIVO
    server.onNotFound([](AsyncWebServerRequest *request) {
        LOG_PRINT("WS: Redirecting... ");
        LOG_PRINTLN(request->url());
        request->redirect("/");
    });
}

String WebServerManager::getDogsJson() {
    // Datos falsos para probar visualmente
    return "[{\"name\":\"Luna (Test)\",\"dog_code\":\"LUNA-001\"},{\"name\":\"Simón (Test)\",\"dog_code\":\"SIMON-02\"}]";
}
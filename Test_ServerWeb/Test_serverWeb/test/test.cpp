#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// --- Credenciales del AP (La red que crea el ESP32) ---
const char* AP_SSID = "Lanzador_Config";
const char* AP_PASS = "12345678"; // Min 8 chars

// Instancia del Servidor en puerto 80
AsyncWebServer server(80);

// --- Helpers ---

// Simula la lectura del DataManager
String getDogsJson() {
    // En el proyecto real: return dataManager->readDogList();
    return "[{\"name\":\"Luna\",\"code\":\"LUNA-001\"},{\"name\":\"Simón\",\"code\":\"SIMON-02\"}]";
}

void setup() {
    Serial.begin(115200);
    
    // 1. Montar LittleFS
    if(!LittleFS.begin()){
        Serial.println("Error montando LittleFS");
        return;
    }

    // 2. Iniciar WiFi en modo Access Point
    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.print("AP Iniciado. IP: ");
    Serial.println(WiFi.softAPIP());

    // 3. --- RUTAS ESTÁTICAS (Archivos) ---
    
    // Si piden "/", servir index.html
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    // 4. --- RUTAS DE API (Datos) ---

    // GET /api/dogs -> Devolver JSON de perros
    server.on("/api/dogs", HTTP_GET, [](AsyncWebServerRequest *request){
        String json = getDogsJson();
        request->send(200, "application/json", json);
        Serial.println("API: Dogs list sent");
    });

    // POST /api/start -> Recibir configuración
    // Nota: ESPAsyncWebServer maneja el body de forma especial
    server.on("/api/start", HTTP_POST, 
        [](AsyncWebServerRequest *request){
            // Esta función se ejecuta al FINAL de la petición
            request->send(200, "text/plain", "OK");
        },
        NULL, // File upload handler (no usamos)
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            // Body handler: Aquí llega el JSON
            
            // Convertir data (bytes) a String
            // (Para JSONs muy grandes se debe hacer por fragmentos, pero para config esto sirve)
            String body = "";
            for(size_t i=0; i<len; i++) body += (char)data[i];
            
            Serial.println("API: Start received!");
            Serial.println(body);

            // Parsear con ArduinoJson
            StaticJsonDocument<512> doc;
            deserializeJson(doc, body);
            
            const char* dog = doc["dog_code"];
            const char* time = doc["timestamp"];
            
            Serial.printf("--> Configurar: Perro=%s, Hora=%s\n", dog, time);
            
            // AQUÍ ENCOLARÍAMOS EL EVENTO A LA FSM
            // stateManager->sendEvent(EVENT_START...);
    });

    // 5. Arrancar servidor
    server.begin();
    Serial.println("Servidor Web Activo");
}

void loop() {
    // Nada aquí. ESPAsyncWebServer es totalmente asíncrono.
    // No bloquea la FSM.
    delay(1000);
}
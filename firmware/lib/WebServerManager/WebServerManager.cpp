/****************************************************************
 * @file WebServerManager.cpp
 * @brief Implements WebServerManager methods.
 ****************************************************************/

#include "WebServerManager.h"
#include "TrainingSession.h"
#include "DataManager.h"
#include "StateManager.h"
#include "Events.h"
#include "config.h"

// --- Constructor ---
WebServerManager::WebServerManager() 
    : dataManager(nullptr), stateManager(nullptr), server(80){
    // Nothing else to init here
}

// --- Public Methods ---
void WebServerManager::begin() {
    LOG_PRINTLN("WS: Starting services...");

    // 1. WiFi AP
    WiFi.softAP(AP_SSID);
    LOG_PRINT("WS: AP IP Address: ");
    LOG_PRINTLN(WiFi.softAPIP());

    // 2. DNS (Captive Portal)
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

    // 3. Web Server
    setupRoutes();
    server.begin();
    LOG_PRINTLN("WS: HTTP Server started.");
}

void WebServerManager::stop() {
    dnsServer.stop();
    server.end();
    WiFi.softAPdisconnect(true);
    LOG_PRINTLN("WS: Services stopped.");
}

void WebServerManager::update() {
    dnsServer.processNextRequest();
}

void WebServerManager::setTargetSession(TrainingSession* session) {
    this->targetSession = session;
}

void WebServerManager::setDataManager(DataManager* dm) {
    this->dataManager = dm;
}

void WebServerManager::setStateManager(StateManager* sm) {
    this->stateManager = sm;
}

// --- Private: Setup Routes ---
void WebServerManager::setupRoutes() {
    
    // 1. Static Files (Frontend)
    // Servimos todo lo que esté en la raíz de LittleFS
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    // 2. API Routes
    // Usamos lambdas simples para capturar 'this' y llamar al método privado

    // GET /api/dogs
    server.on("/api/dogs", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleApiGetDogs(request);
    });

    // GET /api/status
    server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleApiGetStatus(request);
    });

    // POST /api/start (Requiere manejo de Body)
    server.on("/api/start", HTTP_POST, 
        // Handler final (solo responde OK)
        [](AsyncWebServerRequest *request){ request->send(200, "application/json", "{\"status\":\"processing\"}"); },
        NULL,
        // Body Handler (Aquí llegan los datos)
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            this->handleApiPostStart(request, data, len, index, total);
        }
    );

    // 3. Captive Portal Redirect (404 -> index.html)
    server.onNotFound([](AsyncWebServerRequest *request) {
        request->redirect("/");
    });
}

// --- Private: API Handlers Implementation ---
void WebServerManager::handleApiGetDogs(AsyncWebServerRequest *request) {
    // Leemos el archivo JSON directamente del DataManager
    String json = dataManager->readDogList();
    
    // --- DEBUG: IMPRIMIR LO QUE SE LEYÓ ---
    LOG_PRINTLN("--- CONTENIDO LEÍDO DE FLASH (dog_list.json) ---");
    if (json.length() > 0) {
        LOG_PRINTLN(json); // <--- ¡Aquí verás tu JSON real en la consola!
    } else {
        LOG_PRINTLN("[VACÍO] El archivo no existe o está vacío.");
    }
    LOG_PRINTLN("----------------------------------------------");
    // --------------------------------------
    
    request->send(200, "application/json", json);
    LOG_PRINTLN("WS: Served /api/dogs");
}

void WebServerManager::handleApiGetStatus(AsyncWebServerRequest *request) {
    StaticJsonDocument<256> doc;
    
    // Obtenemos datos reales del hardware
    size_t total = 0, used = 0;
    dataManager->getStorageUsage(total, used);
    
    doc["pending_sessions"] = dataManager->countPendingSessions();
    doc["storage_percent"] = (total > 0) ? (int)((used * 100) / total) : 0;
    
    // TODO: Leer batería real del UI
    // doc["battery"] = userInterface->getBatteryPercentage(); 
    doc["battery"] = 100; // Mockup por ahora

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServerManager::handleApiPostStart(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    // 1. Reconstruir JSON (Igual que antes)
    String body = "";
    for(size_t i=0; i<len; i++) body += (char)data[i];
    
    LOG_PRINTLN("WS: Received /api/start Payload:");
    LOG_PRINTLN(body);

    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
        LOG_PRINTLN("WS Error: JSON Parsing Failed!");
        request->send(400, "application/json", "{\"status\":\"error\",\"msg\":\"Invalid JSON\"}");
        return;
    }

    // 2. Extracción de datos
    const char* dogCode = doc["dog_code"] | ""; 
    const char* mode = doc["mode"] | "manual"; 
    float temp = doc["temp"] | 0.0;
    const char* timestamp = doc["timestamp"] | "";

    if (strlen(dogCode) == 0 || strcmp(dogCode, "undefined") == 0) {
        request->send(400, "application/json", "{\"status\":\"error\",\"msg\":\"Select a dog\"}");
        return;
    }

    if (this->targetSession != nullptr) {
        
        // Escribimos directamente en el objeto de ConfigState
        this->targetSession->setDogCode(dogCode);
        
        if (strlen(timestamp) > 0) this->targetSession->setStartedAt(timestamp);

        String conditionsJson = "{\"temp\":" + String(temp, 1) + "}";
        this->targetSession->setConditions(conditionsJson);

        String typeJson = "{\"mode\":\"" + String(mode) + "\"}";
        this->targetSession->setType(typeJson);
        
        LOG_PRINTF("WS: Session configured via pointer for %s\n", dogCode);
    } else {
        LOG_PRINTLN("WS Warning: No target session set via setTargetSession()!");
        // Podríamos responder error 500 aquí si es crítico
    }

    // 4. Enviar Evento a la FSM
    Event ev;
    if (strcmp(mode, "auto") == 0) {
        ev.type = EVENT_START_AUTO_PLAY;
    } else {
        ev.type = EVENT_START_MANUAL_PLAY;
    }
    
    xQueueSend(stateManager->getEventQueue(), &ev, 0);
    request->send(200, "application/json", "{\"status\":\"ok\"}");
}
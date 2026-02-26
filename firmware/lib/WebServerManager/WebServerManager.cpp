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
    : dataManager(nullptr), stateManager(nullptr), server(nullptr), dnsServer(nullptr)
{
    // Nothing else to init here
}

// --- Public Methods ---
void WebServerManager::begin() {
    LOG_PRINTLN("[WEB] Starting services...");

    // 1. WiFi AP
    WiFi.mode(WIFI_AP); // Aseguramos el modo correcto antes de configurar
    IPAddress apIP(192, 168, 4, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(AP_SSID);

    // 2. DNS (Recreación limpia)
    if (dnsServer != nullptr) { delete dnsServer; }
    dnsServer = new DNSServer();
    dnsServer->start(DNS_PORT, "*", WiFi.softAPIP());

    // 3. Web Server (Recreación limpia)
    if (server != nullptr) { delete server; }
    server = new AsyncWebServer(80);
    setupRoutes();  // Ahora configuramos las rutas SIEMPRE
    server->begin();

    LOG_PRINTLN("[WEB] HTTP & DNS Servers started dynamically");
    LOG_PRINTF("[WEB] SSID: %s\n", AP_SSID);
}

void WebServerManager::stop() {
    LOG_PRINTLN("WS: Pausing services...");
    vTaskDelay(pdMS_TO_TICKS(500));

    // Destruir DNS
    if (dnsServer != nullptr) {
        dnsServer->stop();
        delete dnsServer;
        dnsServer = nullptr;
    }
    vTaskDelay(pdMS_TO_TICKS(1500));

    // Destruir Web Server HTTP
    if (server != nullptr) {
        server->end();
        delete server;
        server = nullptr;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    
    vTaskDelay(pdMS_TO_TICKS(100)); 

    LOG_PRINTLN("WS: Services stopped and memory freed.");
}

void WebServerManager::update() {
    if (dnsServer != nullptr) {
        dnsServer->processNextRequest();
    }
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
    
    // ==========================================
    // 1. API ROUTES PRIMERO (Para evitar logs de error)
    // ==========================================
    server->on("/api/dogs", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleApiGetDogs(request);
    });

    server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->handleApiGetStatus(request);
    });

    server->on("/api/start", HTTP_POST, 
        [](AsyncWebServerRequest *request){ }, 
        NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            this->handleApiPostStart(request, data, len, index, total);
        }
    );

    server->on("/api/calibrate", HTTP_POST, [this](AsyncWebServerRequest *request) {
        this->handleApiPostCalibrate(request);
    });

    // ==========================================
    // 2. TRAMPAS PARA PORTAL CAUTIVO (Atrapar antes de buscar archivos)
    // ==========================================
    auto captivePortalRedirect = [](AsyncWebServerRequest *request) {
        request->redirect("http://192.168.4.1/");
    };

    server->on("/generate_204", HTTP_ANY, captivePortalRedirect);        // Android
    server->on("/hotspot-detect.html", HTTP_ANY, captivePortalRedirect); // iOS / Apple
    server->on("/fwlink", HTTP_ANY, captivePortalRedirect);              // Windows

    // ==========================================
    // 3. ARCHIVOS ESTÁTICOS 
    // ==========================================
    // Ahora, si no es una API ni una trampa, buscará en LittleFS
    server->serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    // ==========================================
    // 4. PORTAL CAUTIVO AUTOMÁTICO (onNotFound)
    // ==========================================
    server->onNotFound([](AsyncWebServerRequest *request) {
        String host = request->host();
        
        // Si el usuario ya está en nuestra IP pero pidió algo que no existe, enviamos 404 real.
        if (host == "192.168.4.1") {
            request->send(404, "text/plain", "Error 404: Archivo no encontrado");
        } 
        // Si el celular está preguntando a escondidas si hay internet 
        // LO REDIRIGIMOS a nuestra IP. ¡Esto lanza la ventana del Portal Cautivo!
        else {
            request->redirect("http://192.168.4.1/");
        }
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
    doc["device_code"] = dataManager->getDeviceID();
    
    // TODO: Leer batería real del UI
    // doc["battery"] = userInterface->getBatteryPercentage(); 
    doc["battery"] = 100; // Mockup por ahora

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebServerManager::handleApiPostStart(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    // Reconstruir JSON desde buffer
    String body = "";
    for(size_t i=0; i<len; i++) body += (char)data[i];
    
    LOG_PRINTLN("[API START] Nueva sesion recibida");
    LOG_PRINTLN("Raw JSON payload:");
    LOG_PRINTLN(body);

    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
        LOG_PRINTLN("[ERROR] JSON Parsing fallido");
        request->send(400, "application/json", "{\"status\":\"error\",\"msg\":\"Invalid JSON\"}");
        return;
    }

    // Extraer datos del documento JSON
    const char* dogCode = doc["dog_code"] | ""; 
    const char* mode = doc["mode"] | "manual"; 
    int durationS = doc["duration_s"] | 30;
    const char* typeJson = doc["type_json"] | "";
    const char* timestamp = doc["timestamp"] | "";

    LOG_PRINTLN("[PARSED] Datos extraidos:");
    LOG_PRINTF("  DogCode: %s\n", dogCode);
    LOG_PRINTF("  Mode: %s\n", mode);
    LOG_PRINTF("  Duration: %d segundos\n", durationS);
    LOG_PRINTF("  Timestamp: %s\n", timestamp);
    LOG_PRINTF("  TypeJson: %s\n", typeJson);

    if (strlen(dogCode) == 0 || strcmp(dogCode, "undefined") == 0) {
        LOG_PRINTLN("[ERROR] Dog code vacio o undefined");
        request->send(400, "application/json", "{\"status\":\"error\",\"msg\":\"Select a dog\"}");
        return;
    }

    if (this->targetSession != nullptr) {
        
        // Escribir datos en el objeto de sesion
        this->targetSession->setDogCode(dogCode);
        this->targetSession->setDuration(durationS);
        
        if (strlen(timestamp) > 0) this->targetSession->setStartedAt(timestamp);

        // Condiciones se completan con sensores despues
        String conditionsJson = "{}";
        this->targetSession->setConditions(conditionsJson);

        // Type_json recibido del cliente
        if (strlen(typeJson) > 0) {
            this->targetSession->setType(typeJson);
        }
        
        LOG_PRINTLN("[SUCCESS] Sesion guardada en memoria");
        LOG_PRINTF("  Configuration: dog=%s duration=%ds\n", dogCode, durationS);
    } else {
        LOG_PRINTLN("[WARNING] No target session set");
    }

    // Enviar evento a la maquina de estados
    Event ev;
    if (strcmp(mode, "auto") == 0) {
        ev.type = EVENT_START_AUTO_PLAY;
        LOG_PRINTLN("[EVENT] AUTO_PLAY iniciado");
    } else {
        ev.type = EVENT_START_MANUAL_PLAY;
        LOG_PRINTLN("[EVENT] MANUAL_PLAY iniciado");
    }
    
    xQueueSend(stateManager->getEventQueue(), &ev, 0);
    request->send(200, "application/json", "{\"status\":\"ok\"}");
}

void WebServerManager::handleApiPostCalibrate(AsyncWebServerRequest *request) {
    LOG_PRINTLN("WS: Received /api/calibrate");
    
    // Enviar evento de calibración a la máquina de estados
    Event ev;
    ev.type = EVENT_START_CALIBRATION;
    
    if (stateManager != nullptr) {
        xQueueSend(stateManager->getEventQueue(), &ev, 0);
        request->send(200, "application/json", "{\"status\":\"ok\",\"msg\":\"Calibration started\"}");
        LOG_PRINTLN("WS: Calibration event sent to FSM");
    } else {
        LOG_PRINTLN("WS Error: StateManager is null!");
        request->send(500, "application/json", "{\"status\":\"error\",\"msg\":\"StateManager not initialized\"}");
    }
}
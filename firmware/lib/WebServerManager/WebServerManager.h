/****************************************************************
 * @file WebServerManager.h
 * @brief Manages the Local Web Server (Offline Config),
 * Captive Portal (DNS), and REST API endpoints.
 ****************************************************************/

#ifndef WEB_SERVER_MANAGER_H
#define WEB_SERVER_MANAGER_H

// --- Headers ---
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

// --- Forward Declarations ---
class DataManager;
class StateManager;
class TrainingSession;
class HardwareManager;

// --- Class Definition ---
class WebServerManager {
public:

    WebServerManager();

    /**
     * @brief Starts WiFi AP, DNS Server, and Web Server.
     */
    void begin();

    /**
     * @brief Stops all network services.
     */
    void stop();

    /**
     * @brief Maintenance loop (keeps DNS active).
     * Must be called periodically (e.g. inside ConfigState::update).
     */
    void update();

    /**
     * @brief Establece el objeto de sesión donde se guardarán los datos
     * recibidos por la API /api/start.
     * @param session Puntero a la sesión creada por ConfigState.
     */
    void setTargetSession(TrainingSession* session);

    void setDataManager(DataManager* dm);
    void setStateManager(StateManager* sm);
    void setHardwareManager(HardwareManager* hw);

private:
    // --- Dependencies ---
    DataManager* dataManager;
    StateManager* stateManager;
    HardwareManager* hardwareManager;
    TrainingSession* targetSession;

    // --- Network Objects ---
    AsyncWebServer* server = nullptr;
    DNSServer* dnsServer = nullptr;

    // --- Configuration ---
    const char* AP_SSID = "Lanzador_Config";
    const int DNS_PORT = 53;
    const int HTTP_PORT = 80;
    bool routesConfigured = false;

    // --- Internal Setup Methods ---
    void setupRoutes();

    // --- API Handlers (Private Methods) ---
    // Estas funciones contienen la lógica de cada endpoint.
    
    /**
     * @brief GET /api/dogs
     * Returns the list of dogs from LittleFS.
     */
    void handleApiGetDogs(AsyncWebServerRequest *request);

    /**
     * @brief GET /api/status
     * Returns battery, storage, and session count.
     */
    void handleApiGetStatus(AsyncWebServerRequest *request);

    /**
     * @brief POST /api/start
     * Processes the JSON body to configure and start a session.
     */
    void handleApiPostStart(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);

    /**
     * @brief POST /api/calibrate
     * Triggers the calibration mode.
     */
    void handleApiPostCalibrate(AsyncWebServerRequest *request);
};

#endif // WEB_SERVER_MANAGER_H
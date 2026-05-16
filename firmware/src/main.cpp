/****************************************************************
 * @file main.cpp
 * @brief Main application entry point for the ESP32 Trainer.
 ****************************************************************/

/****************************************************************
 * Headers
 ****************************************************************/
#include <Arduino.h>
#include <esp_system.h>
#include <Wire.h>
#include "config.h"       // For LOG_... macros
#include "StateManager.h" // The FSM
#include "DataManager.h"  // The memory/storage manager
#include "SupabaseClient.h"
#include "HardwareManager.h"
#include "HardwareConfig.h"
#include "Credentials.h"
#include "TrainingSession.h"
#include "WebServerManager.h"
#include "EventLogger.h"   // Event logging system
#include "RssiLogger.h"    // RSSI signal strength logger (optional, compile-time)


/****************************************************************
 * Defines and Constants
 ****************************************************************/
#define STATE_MANAGER_TASK_STACK_SIZE 8192 // 8KB stack for the FSM (increased for stability)
#define STATE_MANAGER_TASK_PRIORITY 1      // Low priority

#define UI_TASK_STACK_SIZE 4096 
#define UI_TASK_PRIORITY 1

#define EVENT_LOGGER_LCD_TASK_STACK_SIZE 2048
#define EVENT_LOGGER_LCD_TASK_PRIORITY 0  // Very low priority

/****************************************************************
 * Global Variables
 ****************************************************************/
StateManager* g_stateManager = nullptr;
DataManager* g_dataManager = nullptr;
SupabaseClient* g_supabaseClient = nullptr;
HardwareManager* g_hardwareManager = nullptr;
WebServerManager* g_webServerManager = nullptr;
EventLogger* g_eventLogger = nullptr;
RssiLogger* g_rssiLogger = nullptr;  // RSSI logger (if RSSI_LOGGER_ENABLED)

RTC_DATA_ATTR uint32_t g_bootMarker = 0;

static bool g_debugManualStarted = false;
static bool g_debugBatteryStopLatched = false;
static bool g_debugFlowInitialized = false;
static bool g_debugFlowFinished = false;
static unsigned long g_debugFlowStartAt = 0;
static unsigned long g_debugLastShotAt = 0;
static uint32_t g_debugShotsFired = 0;

#if TEST_LITTLEFS_CAPACITY == 1
static uint32_t g_lfsTestNextMinuteIndex = 0;
#endif

/****************************************************************
 * Task Function Prototypes
 ****************************************************************/
/**
 * @brief The main FreeRTOS task that runs the StateManager (FSM).
 */
void stateManagerTask(void* parameter);

/**
 * @brief The FreeRTOS task that runs the UserInterface (HW) updates.
 */
void userInterfaceTask(void* parameter);

#if TEST_LANZAMIENTOS == 1
static void runModoDebugTest();
static void finishTestAndReturnToConfig(bool success, const String& detail);
#endif

#if TEST_LITTLEFS_CAPACITY == 1
static bool isLeapYear(int year);
static int getDaysInMonth(int year, int month);
static String buildIsoMinuteFromBase(uint32_t minuteIndex);
static String buildSessionPathFromMinuteIndex(uint32_t minuteIndex);
static bool parseSessionMinuteFromPath(const String& path, int64_t& outMinute);
static bool buildDeterministicSession(uint32_t minuteIndex, String& outIso, String& outJson);
static void printLittleFSCapacityInfo();
static void listSessionNamesAndCheckContinuity();
static void dumpSessionCsvOverUart();
static void runLittleFSCapacityFill();
#endif

static void reportBootCause();

static String buildDirtyTestIso();
static bool saveRawSessionForTest(const String& startedAt,
                                  const String& rawJson,
                                  const char* label);
static void runI2CScanner();

#if EVENT_LOGGER_LCD_ENABLED
/**
 * @brief The FreeRTOS task that updates the LCD with event logs.
 * Low priority, non-blocking.
 */
void eventLoggerLCDTask(void* parameter);
#endif

static void runI2CScanner() {
    LOG_PRINTF("\n[I2C] Scanner start (SDA=%d, SCL=%d)\n", PIN_BME_SDA, PIN_BME_SCL);

    Wire.begin(PIN_BME_SDA, PIN_BME_SCL);
    vTaskDelay(20 / portTICK_PERIOD_MS);

    int found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        uint8_t error = Wire.endTransmission();

        if (error == 0) {
            LOG_PRINTF("[I2C] Found device at 0x%02X\n", addr);
            found++;
        } else if (error == 4) {
            LOG_PRINTF("[I2C] Unknown error at 0x%02X\n", addr);
        }
    }

    if (found == 0) {
        LOG_PRINTLN("[I2C] No I2C devices found.");
    }

    LOG_PRINTLN("[I2C] Scanner end.\n");
}

/****************************************************************
 * Setup Function
 ****************************************************************/
void setup() {
    PIN_MODE(2, OUTPUT); // Example: Set pin 2 as output for debug LED
    PIN_LOW(2);          // Turn on debug LED
    // 1. Initialize Serial Monitor (only in debug mode)
    LOG_SETUP(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS); 
    LOG_PRINTLN("\n--- ESP32 Trainer Firmware ---");

    // 2. Create and initialize DataManager
    g_dataManager = new DataManager();
    if (!g_dataManager->init()) {
        LOG_PRINTLN("FATAL: Failed to initialize DataManager!");
        // ErrorState
        while(1);
    }
    LOG_PRINTLN("DataManager initialized.");
    // Verificar-Opcional: Guardar un ID por defecto si no existe
    String currentDeviceId = g_dataManager->getDeviceID();
    currentDeviceId.trim();
    if (currentDeviceId.length() == 0 || currentDeviceId == "DEFAULT-000") {
        LOG_PRINTLN("Device ID not set. Saving default ID: ESP32-001");
        g_dataManager->saveDeviceID("ESP32-001");
    }
    
    // 3. Create SupabaseClient
    g_supabaseClient = new SupabaseClient(SUPABASE_URL, SUPABASE_API_KEY);
    LOG_PRINTLN("SupabaseClient initialized.");

    // 4. Create and initialize HW_Manager
    g_hardwareManager = new HardwareManager();
    LOG_PRINTLN("UserInterface initialized.");

    // 5. Initialize EventLogger (before WebServer to log startup events)
#if EVENT_LOGGER_ENABLED
    g_eventLogger = EventLogger::getInstance();
    g_eventLogger->begin();
    LOG_PRINTLN("EventLogger initialized.");
    EVENT_INFO("Main Boot OK");
#endif
    
    // 5b. Initialize RssiLogger (optional third output for RSSI data)
#if RSSI_LOGGER_ENABLED
    g_rssiLogger = RssiLogger::getInstance();
    g_rssiLogger->begin();
    LOG_PRINTLN("RssiLogger initialized.");
#endif
    
    reportBootCause();

    // 6. Create WebServerManager
    g_webServerManager = new WebServerManager();
    LOG_PRINTLN("WebServerManager initialized.");

    //7. Create StateManager and inject DataManager dependency
    g_stateManager = new StateManager(g_dataManager, g_supabaseClient, g_hardwareManager, g_webServerManager);
    LOG_PRINTLN("StateManager initialized. Starting FSM...");

    g_webServerManager->setDataManager(g_dataManager);
    g_webServerManager->setStateManager(g_stateManager);
    g_webServerManager->setHardwareManager(g_hardwareManager);
    
    // 7. Initialize HardwareManager with FSM event queue BEFORE StateManager begins
    // This ensures command queue is ready when PowerUpState::enter() sends CMD_LAUNCHER_ON
    g_hardwareManager->init(g_stateManager->getEventQueue(), g_dataManager);

    g_stateManager->begin();

    // 8. Create the StateManager's dedicated task
    xTaskCreate(
        stateManagerTask,               // Task function
        "StateManagerTask",             // Task name (for debugging)
        STATE_MANAGER_TASK_STACK_SIZE,  // Stack size
        NULL,                           // Task parameters
        STATE_MANAGER_TASK_PRIORITY,    // Task priority
        NULL                            // Task handle
    );
    
    // 9. Create the UserInterface's dedicated task
    xTaskCreate(
        userInterfaceTask,              // Task function
        "UserInterfaceTask",            // Task name (for debugging)
        UI_TASK_STACK_SIZE,             // Stack size
        NULL,                           // Task parameters
        UI_TASK_PRIORITY,               // Task priority
        NULL                            // Task handle
    );

#if EVENT_LOGGER_LCD_ENABLED
    // 10. Create EventLogger LCD task (if LCD is enabled)
    xTaskCreate(
        eventLoggerLCDTask,             // Task function
        "EventLoggerLCD",               // Task name (for debugging)
        EVENT_LOGGER_LCD_TASK_STACK_SIZE, // Stack size
        NULL,                           // Task parameters
        EVENT_LOGGER_LCD_TASK_PRIORITY, // Task priority (very low)
        NULL                            // Task handle
    );
    LOG_PRINTLN("EventLogger LCD task created.");
#endif

    LOG_PRINTLN("Setup complete. FSM task is running.");

#if TEST_LANZAMIENTOS == 0
    // --- Instrucciones para el simulador ---
    LOG_PRINTLN("\n--- Event Simulator Ready ---");
    LOG_PRINTLN("Send commands via Serial Monitor (No new line/CR):");
    LOG_PRINTLN("\n--- FSM Events ---");
    LOG_PRINTLN(" 'o' -> EVENT_MODE_ONLINE_ACTIVATED");
    LOG_PRINTLN(" 'f' -> EVENT_MODE_OFFLINE_ACTIVATED");
    LOG_PRINTLN(" 's' -> EVENT_SYNC_COMPLETED (Simulate)");
    LOG_PRINTLN(" 'e' -> EVENT_SYNC_FAILED (Simulate)");
    LOG_PRINTLN(" 'm' -> EVENT_START_MANUAL_PLAY");
    LOG_PRINTLN(" 'c' -> EVENT_START_CALIBRATION");

    LOG_PRINTLN("\n--- Hardware Commands (via HardwareManager) ---");
    LOG_PRINTLN(" 'T' -> TAG power ON (detection mode)");
    LOG_PRINTLN(" 't' -> TAG power OFF");
    LOG_PRINTLN(" 'C' -> TAG calibration mode");
    LOG_PRINTLN(" 'R' -> Remote NRF24 power ON");
    LOG_PRINTLN(" 'r' -> Remote NRF24 power OFF");
    LOG_PRINTLN(" 'L' -> User message: ACTIVE");
    LOG_PRINTLN(" 'E' -> User message: ERROR");
    LOG_PRINTLN(" 'S' -> User message: SUCCESS");
    LOG_PRINTLN(" 'A' -> Solenoid fire");
    LOG_PRINTLN(" 'F' -> Launcher fire");
    LOG_PRINTLN(" 'B' -> BLE Scanner ON");
    LOG_PRINTLN(" 'b' -> BLE Scanner OFF");

    LOG_PRINTLN("\n--- Data Tests ---");
    LOG_PRINTLN(" 'p' -> print numero de trainings pendientes");
    LOG_PRINTLN(" 'v' -> print contenido de TODOS los archivos guardados (DEBUG)");
    LOG_PRINTLN(" 'V' -> Validate all session files and clean if needed (TEST)");
    LOG_PRINTLN(" 'w' -> Write a dummy training session to LittleFS");
    LOG_PRINTLN(" 'h' -> Write a RECOVERABLE dirty session (invalid optional fields)");
    LOG_PRINTLN(" 'H' -> Write an UNRECOVERABLE dirty session (invalid critical fields)");
    LOG_PRINTLN(" 'M' -> Write a mixed block: clean + recoverable + unrecoverable + clean");
    LOG_PRINTLN(" 'I' -> Scan I2C bus (BME wires)");
    LOG_PRINTLN(" 'l' -> List local dog_list.json content");

#if TEST_LITTLEFS_CAPACITY == 1
    LOG_PRINTLN(" 'i' -> LittleFS capacity info + next deterministic ISO");
    LOG_PRINTLN(" 'k' -> List session file names + continuity check");
    LOG_PRINTLN(" 'u' -> Dump /sessions as CSV over UART");
    LOG_PRINTLN(" 'n' -> Save 1 deterministic session (start 2027-01-01T00:00:00.000Z)");
    LOG_PRINTLN(" 'N' -> Save deterministic sessions until failure or max");
    LOG_PRINTLN(" 'y' -> Reset deterministic minute index to 0");
#endif

    LOG_PRINTLN("\n--- Write WIFI ---");
    LOG_PRINTLN(" 'W' -> Write WIFI in NVS");

#if RSSI_LOGGER_ENABLED
    LOG_PRINTLN("\n--- RSSI Logger Commands ---");
#if RSSI_LOGGER_DUMP_ENABLED == 1
    LOG_PRINTLN(" '&' -> rssi-dump (binary framed RAM buffer over UART)");
#endif
    LOG_PRINTLN(" '%' -> rssi-clear (clear RAM buffer)");
    LOG_PRINTLN(" '!' -> rssi-stats (print min/max/avg from RAM buffer)");
#endif

#else
    EVENT_INFO("Main:TEST_LANZAMIENTOS ON");
#endif
    
}

/****************************************************************
 * Task Function Implementations
 ****************************************************************/
void stateManagerTask(void* parameter) {
    while (true) {
        // This loop runs as fast as possible.
        // The current state's execute() method is responsible
        // for blocking/sleeping (e.g., on xQueueReceive)
        // to yield the CPU.
        if (g_stateManager != nullptr) {
            g_stateManager->execute();
        }
    }
}

void userInterfaceTask(void* parameter) {
    while (true) {
        if (g_hardwareManager != nullptr) {
            // Esta función revisa millis() y cambia los pines
            // No bloqueante.
            g_hardwareManager->update();
        }
        vTaskDelay(HardwareManager::LOOP_PERIOD_MS / portTICK_PERIOD_MS);
    }
}

static String buildDirtyTestIso() {
    static uint32_t dirtyCounter = 0;
    uint32_t idx = dirtyCounter++;

    int seconds = static_cast<int>(idx % 60U);
    int millis = static_cast<int>((idx * 37U) % 1000U);

    char iso[32];
    snprintf(iso,
             sizeof(iso),
             "2030-01-01T00:00:%02d.%03dZ",
             seconds,
             millis);
    return String(iso);
}

static bool saveRawSessionForTest(const String& startedAt,
                                  const String& rawJson,
                                  const char* label) {
    SessionSaveStatus saveStatus = SESSION_SAVE_OK;
    String savedPath;
    bool saved = g_dataManager->saveSessionToChunk(rawJson, startedAt, &saveStatus, &savedPath);

    if (saved) {
        LOG_PRINTF("[Dirty Test] %s saved -> %s\n", label, savedPath.c_str());
    } else {
        LOG_PRINTF("[Dirty Test] %s save failed (%s) path=%s\n",
                   label,
                   DataManager::sessionSaveStatusToString(saveStatus),
                   savedPath.c_str());
    }

    return saved;
}

#if EVENT_LOGGER_LCD_ENABLED
/****************************************************************
 * EventLogger LCD Task
 ****************************************************************/
void eventLoggerLCDTask(void* parameter) {
    // Update LCD with latest logs at 5 Hz
    // This is a low-priority task that won't block critical operations
    const TickType_t updateInterval = pdMS_TO_TICKS(200); // 200ms = 5 Hz
    
    while (true) {
        if (g_eventLogger != nullptr) {
            g_eventLogger->updateLCD();
        }
        vTaskDelay(updateInterval);
    }
}
#endif

/****************************************************************
 * Loop Function
 ****************************************************************/
void loop() {
#if TEST_LANZAMIENTOS == 1
    runModoDebugTest();
#else
    // Simulate events via Serial input for testing purposes
    #if DEBUG_MODE == 1
        if (Serial.available() > 0) {
            char command = Serial.read();

            // Asegurarse de que el StateManager ya esté creado
            if (g_stateManager == nullptr) return;
            QueueHandle_t queue = g_stateManager->getEventQueue();
            if (queue == nullptr) return;

            Event event;
            bool sendEvent = true;
            event.type = EVENT_NULL;

            // Variables auxiliares para las pruebas manuales
            TrainingSession dummySession; 
            String jsonStr;

            switch (command) {
                // --- EVENTOS DE LA FSM ---
                case 'o': // Online
                    event.type = EVENT_MODE_ONLINE_ACTIVATED;
                    break;
                case 'f': // Offline
                    event.type = EVENT_MODE_OFFLINE_ACTIVATED;
                    break;
                case 's': // Sync Success
                    event.type = EVENT_SYNC_COMPLETED;
                    break;
                case 'e': // Sync Error/Failed
                    event.type = EVENT_SYNC_FAILED;
                    break;
                case 'm': // Manual Play
                    event.type = EVENT_START_MANUAL_PLAY;
                    break;
                case 'q':
                    event.type = EVENT_PLAY_FINISHED;
                    break;    
                case 'c': // Calibration
                    event.type = EVENT_START_CALIBRATION;
                    break;

                // --- COMANDOS DE HARDWARE (vía HardwareManager) ---
                case 'd': // Calibration
                    event.type = EVENT_DOG_DETECTED;
                    break;
                case 'x': // Dog Lost
                    event.type = EVENT_DOG_LOST;
                    break;

                // --- HARDWARE COMMANDS ---
                case 'T': // TAG ON (detection)
                    g_hardwareManager->sendCommand(CMD_TAG_POWER_ON, 0);
                    sendEvent = false;
                    LOG_PRINTLN("[TEST] CMD_TAG_POWER_ON sent (detection mode)");
                    break;
                case 't': // TAG OFF
                    g_hardwareManager->sendCommand(CMD_TAG_POWER_OFF, 0);
                    sendEvent = false;
                    LOG_PRINTLN("[TEST] CMD_TAG_POWER_OFF sent");
                    break;
                case 'R': // Remote ON
                    g_hardwareManager->sendCommand(CMD_REMOTE_POWER_ON, 0);
                    sendEvent = false;
                    LOG_PRINTLN("[TEST] CMD_REMOTE_POWER_ON sent");
                    break;
                case 'r': // Remote OFF
                    g_hardwareManager->sendCommand(CMD_REMOTE_POWER_OFF, 0);
                    sendEvent = false;
                    LOG_PRINTLN("[TEST] CMD_REMOTE_POWER_OFF sent");
                    break;
                case 'L': // LED Active
                    g_hardwareManager->sendCommand(CMD_MSG_SET, USER_MSG_ACTIVE);
                    sendEvent = false;
                    LOG_PRINTLN("[TEST] CMD_MSG_SET sent (ACTIVE)");
                    break;
                case 'E': // LED Error
                    g_hardwareManager->sendCommand(CMD_MSG_SET, USER_MSG_ERROR);
                    sendEvent = false;
                    LOG_PRINTLN("[TEST] CMD_MSG_SET sent (ERROR)");
                    break;
                case 'S': // LED Success
                    g_hardwareManager->sendCommand(CMD_MSG_SET, USER_MSG_SUCCESS);
                    sendEvent = false;
                    LOG_PRINTLN("[TEST] CMD_MSG_SET sent (SUCCESS)");
                    break;
                case 'A': // Solenoid Fire
                    g_hardwareManager->sendCommand(CMD_SOLENOID_FIRE, 0);
                    sendEvent = false;
                    LOG_PRINTLN("[TEST] CMD_SOLENOID_FIRE sent");
                    break;
               
                // --- PRUEBAS DE DATOS ---
                case 'w': // Write Session (Simular fin de juego)
                    LOG_PRINTLN("\n[Test] Simulating completed training...");
                    // Llenamos datos falsos
                    dummySession.setDogCode("SIMON-01"); // Asegúrate que este perro exista en tu DB o fallará el RPC
                    dummySession.setStartedAt("2025-12-01T10:00:00Z"); 
                    dummySession.setDuration(66);
                    dummySession.setResult("success");
                    dummySession.setConditions("{\"temp\":24}");
                    dummySession.setType("{\"mode\":\"auto\"}");
                    dummySession.setDeviceCode(g_dataManager->getDeviceID()); // Usa el ID real guardado

                    if (dummySession.serialize(jsonStr)) {
                        SessionSaveStatus saveStatus = SESSION_SAVE_OK;
                        String savedPath;
                        if (g_dataManager->saveSessionToChunk(jsonStr, dummySession.getStartedAt(), &saveStatus, &savedPath)) {
                            LOG_PRINTF("[Test] Session saved to LittleFS: %s\n", savedPath.c_str());
                        } else {
                            LOG_PRINTF("[Test] Save failed (%s) path=%s\n",
                                       DataManager::sessionSaveStatusToString(saveStatus),
                                       savedPath.c_str());
                        }
                    }
                break;

                case 'h': // Dirty recoverable session
                    {
                        String startedAt = buildDirtyTestIso();
                        // Valid critical fields + invalid optional values => RECOVERABLE
                        String recoverableJson = String("{") +
                            "\"p_dog_code\":\"FIRU-001\"," +
                            "\"p_started_at\":\"" + startedAt + "\"," +
                            "\"p_result\":\"success\"," +
                            "\"p_device_code\":\"" + g_dataManager->getDeviceID() + "\"," +
                            "\"p_duration_s\":99999," +
                            "\"p_timeout_s\":99999," +
                            "\"p_conditions\":{\"temp\":300.0,\"humidity\":-5.0,\"pressure\":10.0}," +
                            "\"p_type\":\"invalid_type\"" +
                            "}";
                        saveRawSessionForTest(startedAt, recoverableJson, "RECOVERABLE");
                    }
                    sendEvent = false;
                    break;

                case 'H': // Dirty unrecoverable session
                    {
                        String startedAt = buildDirtyTestIso();
                        // Invalid critical fields => UNRECOVERABLE
                        String unrecoverableJson = String("{") +
                            "\"p_dog_code\":\"\"," +
                            "\"p_started_at\":\"" + startedAt + "\"," +
                            "\"p_result\":\"\"," +
                            "\"p_device_code\":\"" + g_dataManager->getDeviceID() + "\"" +
                            "}";
                        saveRawSessionForTest(startedAt, unrecoverableJson, "UNRECOVERABLE");
                    }
                    sendEvent = false;
                    break;

                case 'M': // Mixed block: clean + recoverable + unrecoverable + clean
                    {
                        String deviceCode = g_dataManager->getDeviceID();

                        String s1 = buildDirtyTestIso();
                        String clean1 = String("{") +
                            "\"p_dog_code\":\"FIRU-001\"," +
                            "\"p_started_at\":\"" + s1 + "\"," +
                            "\"p_result\":\"success\"," +
                            "\"p_device_code\":\"" + deviceCode + "\"," +
                            "\"p_duration_s\":90" +
                            "}";

                        String s2 = buildDirtyTestIso();
                        String recoverable = String("{") +
                            "\"p_dog_code\":\"FIRU-001\"," +
                            "\"p_started_at\":\"" + s2 + "\"," +
                            "\"p_result\":\"success\"," +
                            "\"p_device_code\":\"" + deviceCode + "\"," +
                            "\"p_duration_s\":99999," +
                            "\"p_conditions\":{\"temp\":200.0}" +
                            "}";

                        String s3 = buildDirtyTestIso();
                        String unrecoverable = String("{") +
                            "\"p_dog_code\":\"\"," +
                            "\"p_started_at\":\"" + s3 + "\"," +
                            "\"p_result\":\"\"," +
                            "\"p_device_code\":\"" + deviceCode + "\"" +
                            "}";

                        String s4 = buildDirtyTestIso();
                        String clean2 = String("{") +
                            "\"p_dog_code\":\"FIRU-001\"," +
                            "\"p_started_at\":\"" + s4 + "\"," +
                            "\"p_result\":\"success\"," +
                            "\"p_device_code\":\"" + deviceCode + "\"," +
                            "\"p_duration_s\":85" +
                            "}";

                        LOG_PRINTLN("[Dirty Test] Writing mixed block (clean, recoverable, unrecoverable, clean)...");
                        saveRawSessionForTest(s1, clean1, "MIX-CLEAN-1");
                        saveRawSessionForTest(s2, recoverable, "MIX-RECOVERABLE");
                        saveRawSessionForTest(s3, unrecoverable, "MIX-UNRECOVERABLE");
                        saveRawSessionForTest(s4, clean2, "MIX-CLEAN-2");
                    }
                    sendEvent = false;
                    break;

                case 'I': // I2C scanner for BME280 diagnostics
                    runI2CScanner();
                    sendEvent = false;
                    break;

                case 'p': // Status (Ver pendientes)
                    LOG_PRINTF("\n[Test] Pending Sessions: %d\n", g_dataManager->countPendingSessions());
                break;

                case 'v': // View all session files
                    g_dataManager->printAllSessionFiles();
                    sendEvent = false;
                break;

                case 'V': // Validate all session files (TEST)
                    {
                        LOG_PRINTLN("\n[Test] Validating all session files...");
                        SemaphoreHandle_t mutex = g_dataManager->getMutex();
                        if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
                            File root = g_dataManager->openSessionDirectory();
                            if (root) {
                                File file = root.openNextFile();
                                int fileNum = 0;
                                while (file) {
                                    if (!file.isDirectory()) {
                                        fileNum++;
                                        String content = file.readString();
                                        String path = file.path();
                                        LOG_PRINTF("\nFile %d: %s\n", fileNum, path.c_str());
                                        
                                        ValidationResult result = g_dataManager->validateSessionFile(content);
                                        if (result == VALID) {
                                            LOG_PRINTLN("  Status: VALID");
                                        } else if (result == RECOVERABLE) {
                                            LOG_PRINTLN("  Status: RECOVERABLE");
                                            if (g_dataManager->cleanAndSaveSessionFile(path)) {
                                                LOG_PRINTLN("  Cleaned: YES");
                                            }
                                        } else if (result == UNRECOVERABLE) {
                                            LOG_PRINTLN("  Status: UNRECOVERABLE");
                                        }
                                    }
                                    file.close();
                                    file = root.openNextFile();
                                }
                                root.close();
                            }
                            xSemaphoreGive(mutex);
                        }
                    }
                    sendEvent = false;
                break;

                case 'l': // List Dogs (Ver archivo local)
                    LOG_PRINTLN("\n[Test] Reading 'dog_list.json' from LittleFS:");
                    LOG_PRINTLN(g_dataManager->readDogList());
                    LOG_PRINTLN("--- End of List ---");
                break;

                case 'W': // List Dogs (Ver archivo local)
                    LOG_PRINTLN("\n[Test] Write wifi in NVS:");
                    g_dataManager->saveWifiCredentials(WIFI_SSID,WIFI_PASS);
                    LOG_PRINTLN("--- Saved Wifi ---");
                break;        

#if TEST_LITTLEFS_CAPACITY == 1
                case 'i':
                    printLittleFSCapacityInfo();
                    sendEvent = false;
                    break;

                case 'k':
                    listSessionNamesAndCheckContinuity();
                    sendEvent = false;
                    break;

                case 'u':
                    dumpSessionCsvOverUart();
                    sendEvent = false;
                    break;

                case 'n':
                    {
                        String iso;
                        String sessionJson;
                        if (!buildDeterministicSession(g_lfsTestNextMinuteIndex, iso, sessionJson)) {
                            LOG_PRINTLN("[LFS Test] ERROR: Could not build deterministic session JSON.");
                            sendEvent = false;
                            break;
                        }

                        SessionSaveStatus saveStatus = SESSION_SAVE_OK;
                        String savedPath;
                        if (g_dataManager->saveSessionToChunk(sessionJson, iso, &saveStatus, &savedPath)) {
                            LOG_PRINTF("[LFS Test] Saved #%u at %s -> %s\n",
                                       static_cast<unsigned>(g_lfsTestNextMinuteIndex),
                                       iso.c_str(),
                                       savedPath.c_str());
                            g_lfsTestNextMinuteIndex++;
                        } else {
                            LOG_PRINTF("[LFS Test] Save failed at #%u (%s) iso=%s path=%s\n",
                                       static_cast<unsigned>(g_lfsTestNextMinuteIndex),
                                       DataManager::sessionSaveStatusToString(saveStatus),
                                       iso.c_str(),
                                       savedPath.c_str());
                        }
                        printLittleFSCapacityInfo();
                    }
                    sendEvent = false;
                    break;

                case 'N':
                    runLittleFSCapacityFill();
                    sendEvent = false;
                    break;

                case 'y':
                    g_lfsTestNextMinuteIndex = 0;
                    LOG_PRINTLN("[LFS Test] Deterministic minute index reset to 0.");
                    printLittleFSCapacityInfo();
                    sendEvent = false;
                    break;
#endif

                case '-':   //format LittleFS (CUIDADO: Borra todo lo guardado en LittleFS)
                    LOG_PRINTLN("\n[Test] Formatting LittleFS...");
                    if (LittleFS.format()) {
                        LOG_PRINTLN("[Test] LittleFS formatted successfully.");
                    } else {
                        LOG_PRINTLN("[Test] ERROR: Failed to format LittleFS!");
                    }
#if TEST_LITTLEFS_CAPACITY == 1
                    g_lfsTestNextMinuteIndex = 0;
#endif
                    sendEvent = false;
                    break;

#if RSSI_LOGGER_ENABLED
#if RSSI_LOGGER_DUMP_ENABLED == 1
                case '&':
                    // RSSI Logger: Dump log via UART
                    g_rssiLogger->dumpLogViaSUART();
                    sendEvent = false;
                    break;
#endif

                case '%':
                    // RSSI Logger: Clear log file
                    g_rssiLogger->clearLogFile();
                    LOG_PRINTLN("[RSSI] Log file cleared");
                    sendEvent = false;
                    break;

                case '!':
                    // RSSI Logger: Print statistics
                    g_rssiLogger->printStatistics();
                    sendEvent = false;
                    break;
#endif

                default:
                    sendEvent = false;
                    LOG_PRINTF("Simulator: Unknown command '%c'\n", command);
                    break;
            }

            if (sendEvent) {
                LOG_PRINTF("\n[SIMULATOR] Sending Event: %d\n", event.type);
                if (xQueueSend(queue, &event, 0) != pdTRUE) {
                    LOG_PRINTLN("[SIMULATOR] ERROR: Event queue is full!");
                }
            }
        }
    #endif
#endif

    vTaskDelay(50 / portTICK_PERIOD_MS); // Small delay to avoid busy loop
}

#if TEST_LITTLEFS_CAPACITY == 1
static bool isLeapYear(int year) {
    return ((year % 4) == 0 && (year % 100) != 0) || ((year % 400) == 0);
}

static int getDaysInMonth(int year, int month) {
    static const int daysByMonth[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (month == 2 && isLeapYear(year)) {
        return 29;
    }
    return daysByMonth[month - 1];
}

static String buildIsoMinuteFromBase(uint32_t minuteIndex) {
    int year = 2027;
    int month = 1;
    int day = 1;
    int hour = static_cast<int>((minuteIndex / 60U) % 24U);
    int minute = static_cast<int>(minuteIndex % 60U);

    uint32_t daysToAdd = minuteIndex / (24U * 60U);
    while (daysToAdd > 0) {
        int dim = getDaysInMonth(year, month);
        if (day < dim) {
            day++;
        } else {
            day = 1;
            month++;
            if (month > 12) {
                month = 1;
                year++;
            }
        }
        --daysToAdd;
    }

    char iso[32];
    snprintf(iso,
             sizeof(iso),
             "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
             year,
             month,
             day,
             hour,
             minute,
             0,
             0);
    return String(iso);
}

static String buildSessionPathFromMinuteIndex(uint32_t minuteIndex) {
    String iso = buildIsoMinuteFromBase(minuteIndex);
    iso.replace(":", "-");
    iso.replace(".", "-");
    return String("/sessions/") + iso + ".json";
}

static bool parseSessionMinuteFromPath(const String& path, int64_t& outMinute) {
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    int millis = 0;

    int parsed = sscanf(path.c_str(),
                        "/sessions/%d-%d-%dT%d-%d-%d-%dZ.json",
                        &year,
                        &month,
                        &day,
                        &hour,
                        &minute,
                        &second,
                        &millis);
    if (parsed != 7) {
        return false;
    }

    if (month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        return false;
    }

    int dim = getDaysInMonth(year, month);
    if (day > dim) {
        return false;
    }

    int64_t days = 0;
    for (int y = 1970; y < year; ++y) {
        days += isLeapYear(y) ? 366 : 365;
    }
    for (int m = 1; m < month; ++m) {
        days += getDaysInMonth(year, m);
    }
    days += (day - 1);

    outMinute = days * 24LL * 60LL + static_cast<int64_t>(hour) * 60LL + static_cast<int64_t>(minute);
    return true;
}

static bool buildDeterministicSession(uint32_t minuteIndex, String& outIso, String& outJson) {
    outIso = buildIsoMinuteFromBase(minuteIndex);

    TrainingSession session;
    session.setDogCode("FIRU-001");
    session.setStartedAt(outIso);
    session.setDuration(TEST_LITTLEFS_CAPACITY_DURATION_S);
    session.setResult("success");
    session.setConditions("{\"temp\":24.5,\"humidity\":55.0,\"pressure\":1013.2}");
    session.setType("{\"substance\":\"test\",\"distractors\":false,\"context\":\"capacity_test\"}");
    session.setDeviceCode(g_dataManager->getDeviceID());

    return session.serialize(outJson);
}

static void printLittleFSCapacityInfo() {
    size_t totalBytes = 0;
    size_t usedBytes = 0;
    g_dataManager->getStorageUsage(totalBytes, usedBytes);
    size_t freeBytes = (usedBytes <= totalBytes) ? (totalBytes - usedBytes) : 0;

    LOG_PRINTLN("\n[LFS Test] ---- Storage Info ----");
    LOG_PRINTF("[LFS Test] Total: %u bytes\n", static_cast<unsigned>(totalBytes));
    LOG_PRINTF("[LFS Test] Used : %u bytes\n", static_cast<unsigned>(usedBytes));
    LOG_PRINTF("[LFS Test] Free : %u bytes\n", static_cast<unsigned>(freeBytes));
    LOG_PRINTF("[LFS Test] Pending sessions: %d\n", g_dataManager->countPendingSessions());
    LOG_PRINTF("[LFS Test] Next minute index: %u\n", static_cast<unsigned>(g_lfsTestNextMinuteIndex));
    LOG_PRINTF("[LFS Test] Next ISO: %s\n", buildIsoMinuteFromBase(g_lfsTestNextMinuteIndex).c_str());
}

static void listSessionNamesAndCheckContinuity() {
    LOG_PRINTLN("\n[LFS Test] ---- Session Files ----");

    SemaphoreHandle_t mutex = g_dataManager->getMutex();
    if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        LOG_PRINTLN("[LFS Test] ERROR: Could not acquire storage mutex.");
        return;
    }

    File root = g_dataManager->openSessionDirectory();
    if (!root) {
        LOG_PRINTLN("[LFS Test] ERROR: Could not open /sessions directory.");
        xSemaphoreGive(mutex);
        return;
    }

    int totalFiles = 0;
    int parsedFiles = 0;
    int parseErrors = 0;
    int gapCount = 0;
    bool havePrev = false;
    int64_t prevMinute = 0;

    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            totalFiles++;
            String path = String(file.path());
            LOG_PRINTF("[LFS Test] %03d: %s\n", totalFiles, path.c_str());

            int64_t currentMinute = 0;
            if (parseSessionMinuteFromPath(path, currentMinute)) {
                parsedFiles++;
                if (havePrev && currentMinute != (prevMinute + 1LL)) {
                    gapCount++;
                    LOG_PRINTF("[LFS Test]   gap/non-1min jump detected (delta=%d min)\n",
                               static_cast<int>(currentMinute - prevMinute));
                }
                prevMinute = currentMinute;
                havePrev = true;
            } else {
                parseErrors++;
                LOG_PRINTLN("[LFS Test]   warning: filename not parseable as deterministic timestamp");
            }
        }
        file.close();
        file = root.openNextFile();
    }

    root.close();
    xSemaphoreGive(mutex);

    uint32_t consecutiveFromBase = 0;
    while (LittleFS.exists(buildSessionPathFromMinuteIndex(consecutiveFromBase))) {
        consecutiveFromBase++;
    }

    LOG_PRINTLN("[LFS Test] ---- Continuity Summary ----");
    LOG_PRINTF("[LFS Test] Total files in /sessions: %d\n", totalFiles);
    LOG_PRINTF("[LFS Test] Parsed deterministic names: %d\n", parsedFiles);
    LOG_PRINTF("[LFS Test] Parse errors: %d\n", parseErrors);
    LOG_PRINTF("[LFS Test] Non-1min jumps in listed order: %d\n", gapCount);
    LOG_PRINTF("[LFS Test] Consecutive deterministic files from base index 0: %u\n",
               static_cast<unsigned>(consecutiveFromBase));
    LOG_PRINTF("[LFS Test] First missing deterministic index: %u\n",
               static_cast<unsigned>(consecutiveFromBase));
    LOG_PRINTF("[LFS Test] First missing deterministic ISO: %s\n",
               buildIsoMinuteFromBase(consecutiveFromBase).c_str());
}

static void dumpSessionCsvOverUart() {
    LOG_PRINTLN("[LFS CSV] BEGIN");
    LOG_PRINTLN("row,path,size_bytes,deterministic_index,iso_from_name,continuity_from_prev");

    int64_t baseMinute = 0;
    const bool hasBaseMinute = parseSessionMinuteFromPath(buildSessionPathFromMinuteIndex(0), baseMinute);

    SemaphoreHandle_t mutex = g_dataManager->getMutex();
    if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        LOG_PRINTLN("[LFS CSV] ERROR,mutex");
        LOG_PRINTLN("[LFS CSV] END");
        return;
    }

    File root = g_dataManager->openSessionDirectory();
    if (!root) {
        LOG_PRINTLN("[LFS CSV] ERROR,open_sessions_dir");
        xSemaphoreGive(mutex);
        LOG_PRINTLN("[LFS CSV] END");
        return;
    }

    bool havePrev = false;
    int64_t prevMinute = 0;
    int row = 0;

    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            row++;
            String path = String(file.path());
            const size_t sizeBytes = file.size();

            int64_t minuteFromName = 0;
            bool parsed = parseSessionMinuteFromPath(path, minuteFromName);

            String deterministicIndex = "";
            String isoFromName = "";
            String continuity = "";

            if (parsed) {
                if (hasBaseMinute) {
                    deterministicIndex = String(static_cast<long>(minuteFromName - baseMinute));
                }

                int year = 0;
                int month = 0;
                int day = 0;
                int hour = 0;
                int minute = 0;
                int second = 0;
                int millis = 0;
                if (sscanf(path.c_str(),
                           "/sessions/%d-%d-%dT%d-%d-%d-%dZ.json",
                           &year,
                           &month,
                           &day,
                           &hour,
                           &minute,
                           &second,
                           &millis) == 7) {
                    char isoBuf[32];
                    snprintf(isoBuf,
                             sizeof(isoBuf),
                             "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
                             year,
                             month,
                             day,
                             hour,
                             minute,
                             second,
                             millis);
                    isoFromName = String(isoBuf);
                }

                if (havePrev) {
                    continuity = (minuteFromName == (prevMinute + 1LL)) ? "1" : "0";
                }
                prevMinute = minuteFromName;
                havePrev = true;
            }

            LOG_PRINTF("%d,\"%s\",%u,\"%s\",\"%s\",\"%s\"\n",
                       row,
                       path.c_str(),
                       static_cast<unsigned>(sizeBytes),
                       deterministicIndex.c_str(),
                       isoFromName.c_str(),
                       continuity.c_str());
        }

        file.close();
        file = root.openNextFile();
    }

    root.close();
    xSemaphoreGive(mutex);

    LOG_PRINTF("[LFS CSV] ROWS,%d\n", row);
    LOG_PRINTLN("[LFS CSV] END");
}

static void runLittleFSCapacityFill() {
    const uint32_t maxFiles = TEST_LITTLEFS_CAPACITY_MAX_FILES;
    uint32_t createdNow = 0;
    size_t totalBefore = 0;
    size_t usedBefore = 0;
    size_t totalAfter = 0;
    size_t usedAfter = 0;
    int pendingBefore = 0;
    int pendingAfter = 0;
    size_t accumulatedJsonBytes = 0;
    size_t accumulatedStoredBytes = 0;
    SessionSaveStatus stopStatus = SESSION_SAVE_OK;
    uint32_t stopIndex = g_lfsTestNextMinuteIndex;

    g_dataManager->getStorageUsage(totalBefore, usedBefore);
    pendingBefore = g_dataManager->countPendingSessions();

    LOG_PRINTLN("\n[LFS Test] Starting fill process...");
    if (maxFiles == 0) {
        LOG_PRINTLN("[LFS Test] Mode: until failure.");
    } else {
        LOG_PRINTF("[LFS Test] Mode: up to %u new files.\n", static_cast<unsigned>(maxFiles));
    }

    while (true) {
        if (maxFiles > 0 && createdNow >= maxFiles) {
            LOG_PRINTLN("[LFS Test] Reached configured file limit.");
            break;
        }

        String iso;
        String sessionJson;
        if (!buildDeterministicSession(g_lfsTestNextMinuteIndex, iso, sessionJson)) {
            LOG_PRINTLN("[LFS Test] ERROR: Failed to serialize deterministic session.");
            stopStatus = SESSION_SAVE_VERIFY_FAILED;
            stopIndex = g_lfsTestNextMinuteIndex;
            break;
        }

        SessionSaveStatus saveStatus = SESSION_SAVE_OK;
        String savedPath;
        bool saved = g_dataManager->saveSessionToChunk(sessionJson, iso, &saveStatus, &savedPath);
        if (!saved) {
            stopStatus = saveStatus;
            stopIndex = g_lfsTestNextMinuteIndex;
            LOG_PRINTF("[LFS Test] STOP at index=%u status=%s iso=%s path=%s\n",
                       static_cast<unsigned>(g_lfsTestNextMinuteIndex),
                       DataManager::sessionSaveStatusToString(saveStatus),
                       iso.c_str(),
                       savedPath.c_str());
            break;
        }

        g_lfsTestNextMinuteIndex++;
        createdNow++;
        accumulatedJsonBytes += sessionJson.length();

        File storedFile = LittleFS.open(savedPath, "r");
        if (storedFile) {
            accumulatedStoredBytes += storedFile.size();
            storedFile.close();
        }

        if ((createdNow % 25U) == 0U) {
            LOG_PRINTF("[LFS Test] Progress: +%u files, next index=%u\n",
                       static_cast<unsigned>(createdNow),
                       static_cast<unsigned>(g_lfsTestNextMinuteIndex));
        }
    }

    g_dataManager->getStorageUsage(totalAfter, usedAfter);
    pendingAfter = g_dataManager->countPendingSessions();

    size_t usedDelta = (usedAfter >= usedBefore) ? (usedAfter - usedBefore) : 0;
    size_t freeAfter = (usedAfter <= totalAfter) ? (totalAfter - usedAfter) : 0;

    float avgBytesByUsedDelta = 0.0f;
    float avgBytesByJsonSize = 0.0f;
    float avgBytesByStoredSize = 0.0f;
    if (createdNow > 0) {
        avgBytesByUsedDelta = static_cast<float>(usedDelta) / static_cast<float>(createdNow);
        avgBytesByJsonSize = static_cast<float>(accumulatedJsonBytes) / static_cast<float>(createdNow);
        avgBytesByStoredSize = static_cast<float>(accumulatedStoredBytes) / static_cast<float>(createdNow);
    }

    float selectedAvgBytes = 0.0f;
    const char* estimateSource = "none";
    if (avgBytesByUsedDelta > 0.0f) {
        selectedAvgBytes = avgBytesByUsedDelta;
        estimateSource = "used-delta";
    } else if (avgBytesByStoredSize > 0.0f) {
        selectedAvgBytes = avgBytesByStoredSize;
        estimateSource = "stored-size-fallback";
    } else if (avgBytesByJsonSize > 0.0f) {
        selectedAvgBytes = avgBytesByJsonSize;
        estimateSource = "json-size-fallback";
    }

    uint32_t estimatedAdditionalByRealAvg = 0;
    if (selectedAvgBytes > 0.0f) {
        estimatedAdditionalByRealAvg = static_cast<uint32_t>(
            static_cast<float>(freeAfter) / selectedAvgBytes
        );
    }
    uint32_t estimatedTotalByRealAvg = static_cast<uint32_t>(pendingAfter) + estimatedAdditionalByRealAvg;

    LOG_PRINTF("[LFS Test] Fill ended. New files created this run: %u\n",
               static_cast<unsigned>(createdNow));
    LOG_PRINTLN("[LFS Test] ---- Capacity Estimation ----");
    LOG_PRINTF("[LFS Test] Pending before/after: %d -> %d\n", pendingBefore, pendingAfter);
    LOG_PRINTF("[LFS Test] Used delta this run: %u bytes\n", static_cast<unsigned>(usedDelta));
    LOG_PRINTF("[LFS Test] Avg bytes/session (real used delta): %.2f\n", avgBytesByUsedDelta);
    LOG_PRINTF("[LFS Test] Avg bytes/session (stored file size): %.2f\n", avgBytesByStoredSize);
    LOG_PRINTF("[LFS Test] Avg bytes/session (JSON payload only): %.2f\n", avgBytesByJsonSize);
    LOG_PRINTF("[LFS Test] Estimation source: %s\n", estimateSource);

    if (createdNow > 0 && usedDelta == 0) {
        LOG_PRINTLN("[LFS Test] NOTE: usedBytes did not change in this run (LittleFS block granularity). Using fallback estimator.");
    }

    LOG_PRINTF("[LFS Test] Free bytes after run: %u\n", static_cast<unsigned>(freeAfter));
    LOG_PRINTF("[LFS Test] Estimated additional sessions (real avg): %u\n",
               static_cast<unsigned>(estimatedAdditionalByRealAvg));
    LOG_PRINTF("[LFS Test] Estimated total sessions storable (real avg): %u\n",
               static_cast<unsigned>(estimatedTotalByRealAvg));

    if (createdNow == 0) {
        LOG_PRINTF("[LFS Test] Stop reason: %s at index=%u\n",
                   DataManager::sessionSaveStatusToString(stopStatus),
                   static_cast<unsigned>(stopIndex));
    }

    printLittleFSCapacityInfo();
}
#endif

static void reportBootCause() {
    esp_reset_reason_t reason = esp_reset_reason();

    if (reason == ESP_RST_BROWNOUT) {
        EVENT_ERROR("Boot:Brownout");
    } 
    #if TEST_LANZAMIENTOS == 1
    else if (reason == ESP_RST_DEEPSLEEP && g_bootMarker == TEST_LANZAMIENTOS_BOOT_MARKER_BATTERY) {
        EVENT_WARN("Boot:PrevStop BattCritical");
    }
    #endif
    g_bootMarker = 0;
}

#if TEST_LANZAMIENTOS == 1
static void runModoDebugTest() {
    if (g_stateManager == nullptr || g_hardwareManager == nullptr) {
        return;
    }

    QueueHandle_t queue = g_stateManager->getEventQueue();
    if (queue == nullptr) {
        return;
    }

    if (g_debugFlowFinished) {
        return;
    }

    const unsigned long now = millis();
    if (!g_debugFlowInitialized) {
        g_debugFlowInitialized = true;
        g_debugFlowStartAt = now;
        EVENT_INFO("Test:Step1 wait 120s");
        return;
    }

    if (!g_debugManualStarted) {
        if ((now - g_debugFlowStartAt) < TEST_LANZAMIENTOS_SOLENOID_INTERVAL_MS) {
            return;
        }

        Event ev;
        ev.type = EVENT_START_MANUAL_PLAY;
        if (xQueueSend(queue, &ev, 0) == pdTRUE) {
            g_debugManualStarted = true;
            g_debugLastShotAt = now;
            EVENT_INFO("Test:Step1 manual sent");
        } else {
            finishTestAndReturnToConfig(false, "Step1 queue full");
        }
        return;
    }

    const int batteryPercent = g_hardwareManager->getBatteryPercentage();
    if (batteryPercent >= 0 && batteryPercent <= TEST_LANZAMIENTOS_BATTERY_CRITICAL_PERCENT) {
        if (!g_debugBatteryStopLatched) {
            g_debugBatteryStopLatched = true;
            g_bootMarker = TEST_LANZAMIENTOS_BOOT_MARKER_BATTERY;
            finishTestAndReturnToConfig(false, "Batt critical");
        }
        return;
    }

    if ((now - g_debugLastShotAt) >= TEST_LANZAMIENTOS_SOLENOID_INTERVAL_MS) {
        ++g_debugShotsFired;
        const float batteryVoltage = g_hardwareManager->getBatteryVoltage();

        String shotInfo = "Test:Step2 shot #" + String(g_debugShotsFired) +
                          " batt=" + String(batteryPercent) + "%" +
                          " v=" + String(batteryVoltage, 2);
        EVENT_INFO(shotInfo.c_str());

        if (!g_hardwareManager->sendCommand(CMD_SOLENOID_FIRE, 0)) {
            finishTestAndReturnToConfig(false, "Shot cmd fail");
            return;
        }

        g_debugLastShotAt = now;

        if (TEST_LANZAMIENTOS_MAX_DISPAROS > 0 &&
            g_debugShotsFired >= TEST_LANZAMIENTOS_MAX_DISPAROS) {
            finishTestAndReturnToConfig(true, "Max shots reached");
        }
    }
}

static void finishTestAndReturnToConfig(bool success, const String& detail) {
    if (g_debugFlowFinished || g_stateManager == nullptr) {
        return;
    }

    g_debugFlowFinished = true;

    String endMsg = "Test:Step4 end ";
    endMsg += success ? "SUCCESS" : "FAIL";
    if (detail.length() > 0) {
        endMsg += " (" + detail + ")";
    }

    if (success) {
        EVENT_INFO(endMsg.c_str());
    } else {
        EVENT_ERROR(endMsg.c_str());
    }

    QueueHandle_t queue = g_stateManager->getEventQueue();
    if (queue == nullptr) {
        EVENT_ERROR("Test:Step4 no queue");
        return;
    }

    Event finishEvent;
    finishEvent.type = EVENT_PLAY_FINISHED;
    if (xQueueSend(queue, &finishEvent, 0) != pdTRUE) {
        EVENT_ERROR("Test:Step4 queue full");
    } else {
        EVENT_INFO("Test:Step4 -> Config");
    }
}
#endif



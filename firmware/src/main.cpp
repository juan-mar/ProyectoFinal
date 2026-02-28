/****************************************************************
 * @file main.cpp
 * @brief Main application entry point for the ESP32 Trainer.
 ****************************************************************/

/****************************************************************
 * Headers
 ****************************************************************/
#include <Arduino.h>
#include "config.h"       // For LOG_... macros
#include "StateManager.h" // The FSM
#include "DataManager.h"  // The memory/storage manager
#include "SupabaseClient.h"
#include "HardwareManager.h"
#include "Credentials.h"
#include "TrainingSession.h"
#include "WebServerManager.h"


/****************************************************************
 * Defines and Constants
 ****************************************************************/
#define STATE_MANAGER_TASK_STACK_SIZE 4096 // 4KB stack for the FSM
#define STATE_MANAGER_TASK_PRIORITY 1      // Low priority

#define UI_TASK_STACK_SIZE 2048 
#define UI_TASK_PRIORITY 1

/****************************************************************
 * Global Variables
 ****************************************************************/
StateManager* g_stateManager = nullptr;
DataManager* g_dataManager = nullptr;
SupabaseClient* g_supabaseClient = nullptr;
HardwareManager* g_hardwareManager = nullptr;
WebServerManager* g_webServerManager = nullptr;

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
    if (g_dataManager->getDeviceID() == "DEFAULT-000") {
        LOG_PRINTLN("Device ID not set. Saving default ID: ESP32-001");
        g_dataManager->saveDeviceID("ESP32-001");
    }
    
    // 3. Create SupabaseClient
    g_supabaseClient = new SupabaseClient(SUPABASE_URL, SUPABASE_API_KEY);
    LOG_PRINTLN("SupabaseClient initialized.");

    // 4. Create and initialize HW_Manager
    g_hardwareManager = new HardwareManager();
    LOG_PRINTLN("UserInterface initialized.");

    // 5. Create WebServerManager
    g_webServerManager = new WebServerManager();
    LOG_PRINTLN("WebServerManager initialized.");

    //6. Create StateManager and inject DataManager dependency
    g_stateManager = new StateManager(g_dataManager, g_supabaseClient, g_hardwareManager, g_webServerManager);
    LOG_PRINTLN("StateManager initialized. Starting FSM...");

    g_webServerManager->setDataManager(g_dataManager);
    g_webServerManager->setStateManager(g_stateManager);
    
    g_stateManager->begin();
    
    // 7. Initialize UserInterface with FSM event queue
    g_hardwareManager->init(g_stateManager->getEventQueue());

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

    LOG_PRINTLN("Setup complete. FSM task is running.");    
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
    LOG_PRINTLN(" 'L' -> LED pattern: ACTIVE");
    LOG_PRINTLN(" 'l' -> LED pattern: OFF");
    LOG_PRINTLN(" 'E' -> LED pattern: ERROR");
    LOG_PRINTLN(" 'S' -> LED pattern: SUCCESS");
    LOG_PRINTLN(" 'A' -> Solenoid fire");
    LOG_PRINTLN(" 'F' -> Launcher fire");
    LOG_PRINTLN(" 'B' -> BLE Scanner ON");
    LOG_PRINTLN(" 'b' -> BLE Scanner OFF");

    LOG_PRINTLN("\n--- Data Tests ---");
    LOG_PRINTLN(" 'p' -> print numero de trainings pendientes");
    LOG_PRINTLN(" 'w' -> Write a dummy training session to LittleFS");
    LOG_PRINTLN(" 'l' -> List local dog_list.json content");

    LOG_PRINTLN("\n--- Write WIFI ---");
    LOG_PRINTLN(" 'W' -> Write WIFI in NVS");
    
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

/****************************************************************
 * Loop Function
 ****************************************************************/
void loop() {
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
                    g_hardwareManager->sendCommand(CMD_LED_SET_PATTERN, LED_ACTIVE);
                    sendEvent = false;
                    LOG_PRINTLN("[TEST] CMD_LED_SET_PATTERN sent (ACTIVE)");
                    break;
                case 'E': // LED Error
                    g_hardwareManager->sendCommand(CMD_LED_SET_PATTERN, LED_ERROR);
                    sendEvent = false;
                    LOG_PRINTLN("[TEST] CMD_LED_SET_PATTERN sent (ERROR)");
                    break;
                case 'S': // LED Success
                    g_hardwareManager->sendCommand(CMD_LED_SET_PATTERN, LED_SUCCESS);
                    sendEvent = false;
                    LOG_PRINTLN("[TEST] CMD_LED_SET_PATTERN sent (SUCCESS)");
                    break;
                case 'A': // Solenoid Fire
                    g_hardwareManager->sendCommand(CMD_SOLENOID_FIRE, 0);
                    sendEvent = false;
                    LOG_PRINTLN("[TEST] CMD_SOLENOID_FIRE sent");
                    break;
                case 'F': // Launcher Fire
                    g_hardwareManager->sendCommand(CMD_LAUNCHER_FIRE, 0);
                    sendEvent = false;
                    LOG_PRINTLN("[TEST] CMD_LAUNCHER_FIRE sent");
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
                        g_dataManager->saveSessionFile(jsonStr);
                        LOG_PRINTLN("[Test] Session saved to LittleFS.");
                    }
                break;

                case 'p': // Status (Ver pendientes)
                    LOG_PRINTF("\n[Test] Pending Sessions: %d\n", g_dataManager->countPendingSessions());
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
                case '-':   //format LittleFS (CUIDADO: Borra todo lo guardado en LittleFS)
                    LOG_PRINTLN("\n[Test] Formatting LittleFS...");
                    if (LittleFS.format()) {
                        LOG_PRINTLN("[Test] LittleFS formatted successfully.");
                    } else {
                        LOG_PRINTLN("[Test] ERROR: Failed to format LittleFS!");
                    }
                    sendEvent = false;
                    break;

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

    vTaskDelay(50 / portTICK_PERIOD_MS); // Small delay to avoid busy loop
}



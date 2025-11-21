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
#include "Credentials.h"
#include "TrainingSession.h"


/****************************************************************
 * Defines and Constants
 ****************************************************************/
#define STATE_MANAGER_TASK_STACK_SIZE 4096 // 4KB stack for the FSM
#define STATE_MANAGER_TASK_PRIORITY 1      // Low priority

/****************************************************************
 * Global Variables
 ****************************************************************/
/**
 * @brief Global pointer to the main StateManager (FSM).
 */
StateManager* g_stateManager = nullptr;

/**
 * @brief Global pointer to the main DataManager (NVS/FS).
 */
DataManager* g_dataManager = nullptr;

SupabaseClient* g_supabaseClient = nullptr;

/****************************************************************
 * Task Function Prototypes
 ****************************************************************/

/**
 * @brief The main FreeRTOS task that runs the StateManager.
 */
void stateManagerTask(void* parameter);

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
    
    // 2.5 Create SupabaseClient
    g_supabaseClient = new SupabaseClient(SUPABASE_URL, SUPABASE_API_KEY);
    LOG_PRINTLN("SupabaseClient initialized.");

    // Verificar-Opcional: Guardar un ID por defecto si no existe
    if (g_dataManager->getDeviceID() == "DEFAULT-000") {
        LOG_PRINTLN("Device ID not set. Saving default ID: ESP32-001");
        g_dataManager->saveDeviceID("ESP32-001");
    }
    //g_dataManager->saveWifiCredentials(WIFI_SSID,WIFI_PASS);

    // 3. Create StateManager and inject DataManager dependency
    g_stateManager = new StateManager(g_dataManager, g_supabaseClient); 
    LOG_PRINTLN("StateManager initialized. Starting FSM...");
    
    // 4. Create the StateManager's dedicated task
    xTaskCreate(
        stateManagerTask,               // Task function
        "StateManagerTask",             // Task name (for debugging)
        STATE_MANAGER_TASK_STACK_SIZE,  // Stack size
        NULL,                           // Task parameters
        STATE_MANAGER_TASK_PRIORITY,    // Task priority
        NULL                            // Task handle
    );

    LOG_PRINTLN("Setup complete. FSM task is running.");
    
    // --- Instrucciones para el simulador ---
    LOG_PRINTLN("\n--- Event Simulator Ready ---");
    LOG_PRINTLN("Send commands via Serial Monitor (No new line/CR):");
    LOG_PRINTLN(" 'o' -> EVENT_MODE_ONLINE_ACTIVATED");
    LOG_PRINTLN(" 'f' -> EVENT_MODE_OFFLINE_ACTIVATED");
    LOG_PRINTLN(" 's' -> EVENT_SYNC_COMPLETED (Simulate)");
    LOG_PRINTLN(" 'e' -> EVENT_SYNC_FAILED (Simulate)");
    LOG_PRINTLN(" 'p' -> EVENT_START_MANUAL_PLAY (Simulate 'Play')");
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
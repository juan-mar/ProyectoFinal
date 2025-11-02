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

    // 3. Create StateManager and inject DataManager dependency
    g_stateManager = new StateManager(g_dataManager); 
    LOG_PRINTLN("StateManager initialized. Starting FSM...");
    
    // 4. Create the StateManager's dedicated task
    xTaskCreate(
        stateManagerTask,         // Task function
        "StateManagerTask",       // Task name (for debugging)
        STATE_MANAGER_TASK_STACK_SIZE, // Stack size
        NULL,                     // Task parameters
        STATE_MANAGER_TASK_PRIORITY,  // Task priority
        NULL                      // Task handle
    );

    LOG_PRINTLN("Setup complete. FSM task is running.");
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
    // Empty
    vTaskDelay(10);
}
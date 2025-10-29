/****************************************************************
 * @file main.cpp
 * @brief Main application entry point.
 * Initializes the StateManager and provides a serial interface
 * to simulate events for testing the FSM.
 ****************************************************************/

/****************************************************************
 * Headers
 ****************************************************************/
#include <Arduino.h>
#include "StateManager.h" // Our FSM Context
#include "Events.h"       // The shared Event struct and enum

/****************************************************************
 * Defines and Constants
 ****************************************************************/
#define STATE_MANAGER_TASK_STACK_SIZE 4096 // 4KB
#define STATE_MANAGER_TASK_PRIORITY 1

/****************************************************************
 * Global Variables
 ****************************************************************/
/**
 * @brief Global pointer to the main StateManager instance.
 * Created in setup() and used by the FSM task and loop().
 */
StateManager* g_stateManager = nullptr;

/****************************************************************
 * Task Function Prototypes
 ****************************************************************/
/**
 * @brief The main FreeRTOS task that runs the StateManager's
 * execute loop continuously.
 */
void stateManagerTask(void* parameter);

/****************************************************************
 * Setup Function
 ****************************************************************/

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);

    // 1. Initialize Serial Monitor
    Serial.begin(115200);
    // Wait a moment for the monitor to connect
    vTaskDelay(1000 / portTICK_PERIOD_MS); 
    Serial.println("\n--- FSM Test Rig Initializing ---");

    // 2. Create the StateManager instance
    // (Its constructor creates the event queue and the initial ConfigState)
    g_stateManager = new StateManager();

    if (g_stateManager == nullptr) {
        Serial.println("FATAL ERROR: Failed to create StateManager!");
        while(1);
    }

    // 3. Create the StateManager's dedicated task
    xTaskCreate(
        stateManagerTask,         // Task function
        "StateManagerTask",       // Task name (for debugging)
        STATE_MANAGER_TASK_STACK_SIZE, // Stack size
        NULL,                     // Task parameters
        STATE_MANAGER_TASK_PRIORITY,  // Task priority
        NULL                      // Task handle (not needed)
    );

    Serial.println("StateManager task started successfully.");
    Serial.println("\n--- Event Simulation Ready ---");
    Serial.println("Send commands via Serial Monitor (No new line/CR):");
    Serial.println(" 'o' -> Simulate switch to ONLINE");
    Serial.println(" 'f' -> Simulate switch to OFFLINE");
    Serial.println(" 's' -> Simulate SYNC_COMPLETED");
}

/****************************************************************
 * Main FSM Task
 ****************************************************************/

void stateManagerTask(void* parameter) {
    Serial.println("[StateManagerTask] Task running.");
    while (true) {
        // 4. Continuously run the StateManager's execute loop
        if (g_stateManager != nullptr) {
            g_stateManager->execute();
        }

        // 5. Yield to other tasks.
        // This delay controls how often the FSM execute() runs.
        //vTaskDelay(10 / portTICK_PERIOD_MS); // Run every 10ms
    }
}

/****************************************************************
 * Loop Function (Event Simulator)
 ****************************************************************/

void loop() {
    // This loop() acts as our event simulator.
    // It runs in parallel to the stateManagerTask.

    if (Serial.available() > 0) {
        // 6. Read the command character from Serial
        char command = Serial.read();

        // Ensure we have a valid StateManager and queue
        if (g_stateManager == nullptr) return;
        QueueHandle_t queue = g_stateManager->getEventQueue();
        if (queue == nullptr) return;

        // 7. Create the event on the stack
        Event event;
        bool sendEvent = true;

        // 8. Translate the character to an Event
        switch (command) {
            case 'o':
                event.type = EVENT_MODE_ONLINE_ACTIVATED;
                Serial.println("\n[SIMULATOR] Sending EVENT_MODE_ONLINE_ACTIVATED...");
                break;
            
            case 'f':
                event.type = EVENT_MODE_OFFLINE_ACTIVATED;
                Serial.println("\n[SIMULATOR] Sending EVENT_MODE_OFFLINE_ACTIVATED...");
                break;
            
            case 's':
                event.type = EVENT_SYNC_COMPLETED;
                Serial.println("\n[SIMULATOR] Sending EVENT_SYNC_COMPLETED...");
                break;

            default:
                sendEvent = false;
                Serial.printf("[SIMULATOR] Unknown command: '%c'\n", command);
                break;
        }

        // 9. Send a *copy* of the event to the queue
        if (sendEvent) {
            if (xQueueSend(queue, &event, 0) != pdTRUE) {
                Serial.println("[SIMULATOR] ERROR: Event queue is full!");
            }
        }
    }

    //digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));   // turn the LED on (HIGH is the voltage level)
    // Give other tasks (like the Serial task) time to run
    vTaskDelay(50 / portTICK_PERIOD_MS);
}
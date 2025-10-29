/****************************************************************
 * @file ConfigState.cpp
 * @brief Implements the ConfigState class logic.
 ****************************************************************/

 /****************************************************************
 * Headers
 ****************************************************************/
#include "ConfigState.h"
#include "StateManager.h"
#include "Events.h"
#include <Arduino.h> // For Serial

// Include headers for states we can transition to
#include "SyncState.h"
// #include "ManualPlayState.h"  // Uncomment when created
// #include "AutoPlayState.h"    // Uncomment when created


/****************************************************************
 * Methods Implementation / Function Definitions
 ****************************************************************/

void ConfigState::enter(StateManager* manager) {
    Serial.println("Entering ConfigState...");
    // TODO: Initialize and start the WebServerManager here
}

void ConfigState::execute(StateManager* manager) {
    // This state's logic is event-driven. We check the queue.
    Event event;
    QueueHandle_t queue = manager->getEventQueue();

    // Check for an event (non-blocking)
    if (xQueueReceive(queue, &event, 500 / portTICK_PERIOD_MS) == pdTRUE) {
        switch (event.type) {
            case EVENT_MODE_ONLINE_ACTIVATED:
                Serial.println("[ConfigState] Event: Mode ONLINE. Changing to SyncState.");
                manager->changeState(new SyncState());
                break;

            case EVENT_START_MANUAL_PLAY:
                Serial.println("[ConfigState] Event: Start Manual Play.");
                // manager->changeState(new ManualPlayState()); // TODO
                break;

            case EVENT_START_AUTO_PLAY:
                Serial.println("[ConfigState] Event: Start Auto Play.");
                // manager->changeState(new AutoPlayState()); // TODO
                break;
            
            default:
                // Ignore other events
                break;
        }
    }
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));   // turn the LED on (HIGH is the voltage level)

    // TODO: In a real implementation, you would also call:
    // webServerManager->handleClient();
    // ...if your web server is not fully asynchronous.
}

void ConfigState::exit(StateManager* manager) {
    Serial.println("Exiting ConfigState...");
    // TODO: Stop and shut down the WebServerManager here
}
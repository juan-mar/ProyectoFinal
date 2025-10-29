/****************************************************************
 * @file SyncState.cpp
 * @brief Implements the SyncState class logic.
 ****************************************************************/

/****************************************************************
 * Headers
 ****************************************************************/
#include "SyncState.h"
#include "StateManager.h"
#include "Events.h"
#include <Arduino.h> // For Serial

// Include headers for states we can transition to
#include "ConfigState.h"
// #include "IdleState.h" // Uncomment when created


/****************************************************************
 * Methods Implementation / Function Definitions
 ****************************************************************/
void SyncState::enter(StateManager* manager) {
    Serial.println("Entering SyncState...");
    // TODO:
    // 1. Start WiFi connection (client mode)
    // 2. On successful connection, trigger SupabaseManager to start sync
    // 3. For now, we just wait for events.
}

void SyncState::execute(StateManager* manager) {
    // This state's logic is also event-driven
    Event event;
    QueueHandle_t queue = manager->getEventQueue();

    // Check for an event (non-blocking)
    if (xQueueReceive(queue, &event, 0) == pdTRUE) {
        
        switch (event.type) {
            case EVENT_MODE_OFFLINE_ACTIVATED:
                Serial.println("[SyncState] Event: Mode OFFLINE. Changing to ConfigState.");
                manager->changeState(new ConfigState());
                break;

            case EVENT_SYNC_COMPLETED:
                Serial.println("[SyncState] Event: Sync Completed.");
                // manager->changeState(new IdleState()); // TODO
                break;

            case EVENT_SYNC_FAILED:
                Serial.println("[SyncState] Event: Sync FAILED.");
                // manager->changeState(new IdleState()); // TODO
                break;
            
            default:
                // Ignore other events
                break;
        }
    }
}

void SyncState::exit(StateManager* manager) {
    Serial.println("Exiting SyncState...");
    // TODO:
    // 1. Cancel any ongoing sync
    // 2. Disconnect WiFi
}
/****************************************************************
 * @file SyncState.cpp
 * @brief Implements the SyncState class logic.
 ****************************************************************/

/****************************************************************
 * Headers
 ****************************************************************/
#include "SyncState.h"
#include "StateManager.h"
//#include "DataManager.h"
#include "Events.h"
#include "config.h"

// States we can transition to
#include "ConfigState.h"
#include "IdleState.h"


/****************************************************************
 * Methods Implementation / Function Definitions
 ****************************************************************/
SyncState::SyncState(DataManager* dataManager) 
    : dataManager(dataManager)
{
    // Constructor stores the pointer
}

void SyncState::enter(StateManager* manager) {
    LOG_PRINTLN("Entering SyncState...");
    
    // In a real implementation, we would start the WiFi connection here.
    // For now, we'll just simulate starting the sync.
    // wifiManager->connect(dataManager->getWifiSSID(), ...);
    
    // Since we don't have WiFi events yet, we'll trigger the sync logic directly.
    // In the future, this would be called by a "WIFI_CONNECTED" event.
    beginSynchronization(manager);
}

void SyncState::execute(StateManager* manager) {
   Event event;
    QueueHandle_t queue = manager->getEventQueue();

    // This state is purely reactive. It sleeps indefinitely (0% CPU)
    // waiting for an event from the interrupt or the sync process.
    if (xQueueReceive(queue, &event, portMAX_DELAY) == pdTRUE) {
        handleEvent(manager, event);
    }
    
    // update() is not called, as this state has no periodic logic.

}

void SyncState::exit(StateManager* manager) {
    LOG_PRINTLN("Exiting SyncState...");
    // TODO:
    // 1. Cancel any ongoing sync
    // 2. Disconnect WiFi
    // wifiManager->disconnect();
}

/****************************************************************
 * Protected Methods
 ****************************************************************/

void SyncState::handleEvent(StateManager* manager, Event& event) {
    switch (event.type) {
        case EVENT_MODE_OFFLINE_ACTIVATED:
            LOG_PRINTLN("[SyncState] Event: Mode OFFLINE. Changing to ConfigState.");
            manager->changeState(new ConfigState(dataManager));
            break;

        case EVENT_SYNC_COMPLETED:
            LOG_PRINTLN("[SyncState] Event: Sync Completed. Changing to IdleState.");
            manager->changeState(new IdleState(dataManager));
            break;

        case EVENT_SYNC_FAILED:
            LOG_PRINTLN("[SyncState] Event: Sync FAILED. Changing to IdleState.");
            // We still go to Idle. We'll try again next time.
            manager->changeState(new IdleState(dataManager));
            break;
        
        default:
            break;
    }
}

void SyncState::update(StateManager* manager) {
    //foo
}

/****************************************************************
 * Private Methods
 ****************************************************************/

void SyncState::beginSynchronization(StateManager* manager) {
    LOG_PRINTLN("[SyncState] Starting synchronization process...");
    
    // This function will become complex. It will:
    // 1. Call dataManager->supabaseLogin()
    // 2. If login OK, call dataManager->syncSessions()
    // 3. If sync OK, call dataManager->fetchDogList()
    // 4. Finally, send an event *to itself* to signal completion.
    
    // --- For testing, we just simulate a completion event ---
    Event syncEvent;
    syncEvent.type = EVENT_SYNC_COMPLETED;
    // We send the event to our own queue to be processed by execute()
    xQueueSend(manager->getEventQueue(), &syncEvent, 0);
}
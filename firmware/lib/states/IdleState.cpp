/****************************************************************
 * @file IdleState.cpp
 * @brief Implements the IdleState logic for light sleep.
 ****************************************************************/

/****************************************************************
 * Headers
 ****************************************************************/
#include "IdleState.h"
#include "StateManager.h"
#include "DataManager.h"
#include "Events.h"
#include "config.h"
#include "esp_sleep.h" // Required for light sleep functions
#include "SupabaseClient.h"
#include "HardwareManager.h"
#include "EventLogger.h"

// States we can transition to
#include "PowerUpState.h"
#include "SyncState.h"
#include "PowerOffState.h"

/****************************************************************
 * Defines and Constants
 ****************************************************************/
// Idle wakeup sources configured in HardwareManager:
//   - Power Switch OFF → PowerOffState
//   - USB Connected → PowerOffState
//   - Mode Switch change → SyncState (ONLINE) or PowerUpState (OFFLINE)

/****************************************************************
 * Class Method Implementations
 ****************************************************************/

IdleState::IdleState(){
}

void IdleState::enter(StateManager* manager) {
    LOG_PRINTLN("FSM: Entering IdleState...");

    manager->getHardwareManager()->sendCommand(CMD_MSG_OFF, 0);

    Event wakeupEvent = manager->getHardwareManager()->enterLightSleep();

    LOG_PRINTF("FSM: Woke up with event type: %d\n", wakeupEvent.type);
    
    xQueueSend(manager->getEventQueue(), &wakeupEvent, 0);
    LOG_PRINTLN("FSM: Event sent to queue, exiting enter() method.");
}

void IdleState::execute(StateManager* manager) {
    Event event;
    QueueHandle_t queue = manager->getEventQueue();

    if (xQueueReceive(queue, &event, portMAX_DELAY) == pdTRUE) {
        handleEvent(manager, event);
    }
}

void IdleState::exit(StateManager* manager) {
    LOG_PRINTLN("Exiting IdleState...");
    
    manager->getHardwareManager()->prepareForWakeUp();
}

/****************************************************************
 * Protected Methods
 ****************************************************************/

void IdleState::handleEvent(StateManager* manager, Event& event) {
    switch (event.type) {
        case EVENT_POWER_SWITCH_OFF:
            LOG_PRINTLN("[IdleState] Event: Power Switch OFF. Changing to PowerOffState.");
            EVENT_WARN("Idl:PWR->PwOff");
            manager->changeState(new PowerOffState());
            break;
            
        case EVENT_USB_CONNECTED:
            LOG_PRINTLN("[IdleState] Event: USB Connected. Changing to PowerOffState.");
            EVENT_WARN("Idl:USB->PwOff");
            manager->changeState(new PowerOffState());
            break;
            
        case EVENT_MODE_OFFLINE_ACTIVATED:
            LOG_PRINTLN("[IdleState] Event: Mode OFFLINE. Changing to PowerUpState.");
            manager->changeState(new PowerUpState());
            break;
            
        case EVENT_MODE_ONLINE_ACTIVATED:
            LOG_PRINTLN("[IdleState] Event: Mode ONLINE. Changing to SyncState.");
            manager->changeState(new SyncState(manager->getDataManager(), manager->getSupabaseClient()));
            break;
            
        default:
            break;
    }
}

void IdleState::update(StateManager* manager) {

}
/****************************************************************
 * @file PowerUpState.cpp
 * @brief Implements the PowerUpState class logic.
 * 
 * PowerUpState activates the launcher at startup and waits for
 * a configured duration to allow the power supply to stabilize.
 * This prevents brownout conditions that occur when launcher
 * power consumption collides with WiFi transmission peaks.
 * 
 * Timeline:
 *   - enter(): Activate launcher (CMD_LAUNCHER_ON)
 *   - execute(): Wait for POWERUP_STATE_DURATION_MS
 *   - exit(): Transition to ConfigState (or SyncState if ONLINE mode)
 ****************************************************************/

#include "PowerUpState.h"
#include "StateManager.h"
#include "DataManager.h"
#include "Events.h"
#include "config.h"
#include "HardwareManager.h"
#include "HardwareConfig.h"
#include "EventLogger.h"

// States we can transition to
#include "ConfigState.h"
#include "SyncState.h"
#include "PowerOffState.h"

/****************************************************************
 * Constructor
 ****************************************************************/

PowerUpState::PowerUpState() : stateStartTime(0) {}

/****************************************************************
 * State Methods
 ****************************************************************/

void PowerUpState::enter(StateManager* manager) {
    LOG_PRINTLN("[FSM] Entering PowerUpState...");
    stateStartTime = millis();
    manager->getHardwareManager()->sendCommand(CMD_MSG_SET, USER_MSG_POWERING_UP);

    // Activate launcher - primary responsibility of this state
    LOG_PRINTLN("[PowerUp] Activating launcher (CMD_LAUNCHER_ON)...");
    manager->getHardwareManager()->sendCommand(CMD_LAUNCHER_ON, 0);
    
    EVENT_INFO("PwUp:Stabilizing");
}

void PowerUpState::execute(StateManager* manager) {
    // Check elapsed time
    uint32_t elapsed = millis() - stateStartTime;
    
    // After duration expires, transition to ConfigState
    if (elapsed >= POWERUP_STATE_DURATION_MS) {
        LOG_PRINTF("[PowerUp] Duration %lu ms reached. Transitioning to ConfigState.\n", elapsed);
        manager->changeState(new ConfigState(
            manager->getDataManager(), 
            manager->getWebServerManager()
        ));
        return;  // Exit this iteration
    }
    
    // Non-blocking event check during waiting period
    // This allows mode changes to interrupt the power-up sequence
    Event event;
    if (xQueueReceive(manager->getEventQueue(), &event, 0) == pdTRUE) {
        handleEvent(manager, event);
    }
}

void PowerUpState::exit(StateManager* manager) {
    LOG_PRINTLN("[PowerUp] Exiting PowerUpState...");
}

/****************************************************************
 * Event Handling
 ****************************************************************/

void PowerUpState::handleEvent(StateManager* manager, Event& event) {
    switch (event.type) {
        case EVENT_MODE_ONLINE_ACTIVATED:
            // User switched to ONLINE mode during power-up
            // Skip ConfigState and go directly to SyncState
            LOG_PRINTLN("[PowerUp] Event: Mode ONLINE detected (interrupting). Jumping to SyncState...");
            EVENT_INFO("PwUp:->Sync");
            manager->changeState(new SyncState(
                manager->getDataManager(), 
                manager->getSupabaseClient()
            ));
            break;
            
        case EVENT_MODE_OFFLINE_ACTIVATED:
            // Already in OFFLINE mode, ignore
            LOG_PRINTLN("[PowerUp] Event: Mode OFFLINE (already active). Continuing power-up...");
            break;
            
        case EVENT_USB_CONNECTED:
            LOG_PRINTLN("[PowerUp] Event: USB Connected. Changing to PowerOffState.");
            EVENT_WARN("PwUp:USB->PwOff");
            manager->changeState(new PowerOffState());
            break;
            
        case EVENT_POWER_SWITCH_OFF:
            LOG_PRINTLN("[PowerUp] Event: Power Switch OFF. Changing to PowerOffState.");
            EVENT_WARN("PwUp:PWR->PwOff");
            manager->changeState(new PowerOffState());
            break;
            
        default:
            // Ignore other events during power-up
            break;
    }
}

void PowerUpState::update(StateManager* manager) {
    // PowerUpState is purely time-based, no periodic update needed
}

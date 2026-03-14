/****************************************************************
 * @file PowerOffState.cpp
 * @brief Implements the PowerOffState logic for shutdown and deep sleep.
 ****************************************************************/

#include "PowerOffState.h"
#include "StateManager.h"
#include "HardwareManager.h"
#include "config.h"
#include "EventLogger.h"
#include "IdleState.h"
#include <esp_sleep.h>
#include <driver/gpio.h>

PowerOffState::PowerOffState() {
}

void PowerOffState::enter(StateManager* manager) {
    LOG_PRINTLN("[FSM] Entering PowerOffState...");
    EVENT_WARN("PwOff:Shutdown");
    
    // Notify HardwareManager that we're in PowerOffState
    // This enables special GPIO monitoring for USB and Power Switch events
    manager->getHardwareManager()->notifyPowerOffState(true);
    
    LOG_PRINTLN("[PowerOff] Waiting for user action...");
    LOG_PRINTLN("[PowerOff] - Release Power Switch (GPIO14 LOW) to enter deep sleep");
    LOG_PRINTLN("[PowerOff] - Disconnect USB to abort and return to Idle");
}

void PowerOffState::execute(StateManager* manager) {
    // Wait for events from HardwareManager
    // The state doesn't access GPIO directly - all GPIO monitoring is done in HardwareManager::checkGPIOStatus()
    Event event;
    
    if (xQueueReceive(manager->getEventQueue(), &event, pdMS_TO_TICKS(50)) == pdTRUE) {
        handleEvent(manager, event);
    }
}

void PowerOffState::handleEvent(StateManager* manager, Event& event) {
    switch (event.type) {
        case EVENT_POWEROFF_USB_DISCONNECTED:
            // USB was disconnected while waiting in PowerOff
            LOG_PRINTLN("[PowerOff] USB disconnected! Aborting shutdown...");
            EVENT_INFO("PwOff:USB Cancel");
            
            // Disable PowerOff monitoring and return to Idle
            manager->getHardwareManager()->notifyPowerOffState(false);
            manager->changeState(new IdleState());
            break;
            
        case EVENT_POWEROFF_READY_TO_SLEEP:
                // Power Switch released (GPIO14 LOW) - ready for deep sleep
            LOG_PRINTLN("[PowerOff] Power Switch released. Entering deep sleep...");
            EVENT_INFO("PwOff:Sleeping");
            
            // Disable PowerOff monitoring
            manager->getHardwareManager()->notifyPowerOffState(false);
            
            // Perform deep sleep
            // NOTE: This function does NOT return - ESP32 will reset on wakeup
            manager->getHardwareManager()->enterDeepSleep();
            break;
            
        default:
            // Ignore other events in this state
            break;
    }
}

void PowerOffState::exit(StateManager* manager) {
    // Disable PowerOff monitoring if we somehow exit this state abnormally
    LOG_PRINTLN("[PowerOffState] Exiting (unexpected)...");
    manager->getHardwareManager()->notifyPowerOffState(false);
}

void PowerOffState::update(StateManager* manager) {
    // No periodic updates needed - all logic driven by GPIO changes from HardwareManager
}

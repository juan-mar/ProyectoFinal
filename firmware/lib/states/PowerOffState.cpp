/****************************************************************
 * @file PowerOffState.cpp
 * @brief Implements the PowerOffState logic for shutdown and deep sleep.
 ****************************************************************/

#include "PowerOffState.h"
#include "StateManager.h"
#include "HardwareManager.h"
#include "config.h"
#include "EventLogger.h"
#include <esp_sleep.h>
#include <driver/gpio.h>

PowerOffState::PowerOffState() {
}

void PowerOffState::enter(StateManager* manager) {
    LOG_PRINTLN("[FSM] Entering PowerOffState...");
    EVENT_WARN("PwOff:Shutdown");

    // Delegate shutdown and deep sleep to HardwareManager
    manager->getHardwareManager()->enterDeepSleep();
    
    // Note: Code after enterDeepSleep() will never execute
    // Device will reset on wakeup
}

void PowerOffState::execute(StateManager* manager) {
    // This method should never be called - enterDeepSleep() never returns
    LOG_PRINTLN("[PowerOffState] ERROR: execute() called - should not happen!");
    
    vTaskDelay(pdMS_TO_TICKS(1000));
}

void PowerOffState::exit(StateManager* manager) {
    // This method should never be called in normal operation
    LOG_PRINTLN("[PowerOffState] Exiting (unexpected)...");
}

void PowerOffState::handleEvent(StateManager* manager, Event& event) {
    // No events should be processed in this state
}

void PowerOffState::update(StateManager* manager) {
    // No periodic updates in this state
}

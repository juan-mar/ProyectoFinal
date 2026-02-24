/****************************************************************
 * @file CalibrationState.cpp
 * @brief Implements the CalibrationState class logic.
 ****************************************************************/

/****************************************************************
 * Headers
 ****************************************************************/
#include "CalibrationState.h"
#include "StateManager.h"
#include "DataManager.h"
#include "Events.h"
#include "Config.h"
#include "HardwareManager.h"

// States we can transition to
#include "ConfigState.h"

/****************************************************************
 * Defines and Constants
 ****************************************************************/
/**
 * @brief The "tick rate" for this state in milliseconds.
 * How often the update() loop will run during calibration.
 */
#define CALIBRATION_STATE_TICK_MS 50

/**
 * @brief Calibration timeout in milliseconds (1 minute).
 * If calibration doesn't complete within this time, it fails.
 */
#define CALIBRATION_TIMEOUT_MS (60 * 1000)  // 60 seconds

/****************************************************************
 * Class Method Implementations
 ****************************************************************/

CalibrationState::CalibrationState(DataManager* dataManager, HardwareManager* hardwareManager) 
    : dataManager(dataManager), hardwareManager(hardwareManager) 
{}

void CalibrationState::enter(StateManager* manager) {
    LOG_PRINTLN("Entering CalibrationState...");

    hardwareManager->sendCommand(CMD_TAG_POWER_ON, CMD_TAG_PARAM_CALIBRATION);

    // Record the time when calibration started
    calibrationStartTime = millis();
}

void CalibrationState::execute(StateManager* manager) {
    Event event;
    QueueHandle_t queue = manager->getEventQueue();

    if (xQueueReceive(queue, &event, CALIBRATION_STATE_TICK_MS / portTICK_PERIOD_MS) == pdTRUE) {
        handleEvent(manager, event);
    }

    update(manager);
}

void CalibrationState::exit(StateManager* manager) {
    LOG_PRINTLN("Exiting CalibrationState...");

    hardwareManager->sendCommand(CMD_TAG_POWER_OFF);
    vTaskDelay(pdMS_TO_TICKS(500));
}

/****************************************************************
 * Protected Methods
 ****************************************************************/

void CalibrationState::handleEvent(StateManager* manager, Event& event) {
    switch (event.type) {
        case EVENT_CALIBRATION_COMPLETE:
            LOG_PRINTLN("[CalibrationState] Event: Calibration Complete. Returning to ConfigState.");
            manager->changeState(new ConfigState(dataManager, manager->getWebServerManager()));
            break;

        case EVENT_CALIBRATION_CANCEL:
            LOG_PRINTLN("[CalibrationState] Event: Calibration Cancelled. Returning to ConfigState.");
            manager->changeState(new ConfigState(dataManager, manager->getWebServerManager()));
            break;

        case EVENT_CALIBRATION_FAILED:
            LOG_PRINTLN("[CalibrationState] Event: Calibration Failed. Returning to ConfigState.");
            manager->changeState(new ConfigState(dataManager, manager->getWebServerManager()));
            break;
        
        default:
            break;
    }
}

void CalibrationState::update(StateManager* manager) {
    //TODO: Implement calibration logic updates
    
    // Check calibration timeout
    unsigned long elapsedTime = millis() - calibrationStartTime;
    
    if (elapsedTime >= CALIBRATION_TIMEOUT_MS) {
        LOG_PRINTLN("[CalibrationState] Calibration timeout! No completion within time limit.");
        Event timeoutEvent;
        timeoutEvent.type = EVENT_CALIBRATION_FAILED;
        xQueueSend(manager->getEventQueue(), &timeoutEvent, 0);
    }
}

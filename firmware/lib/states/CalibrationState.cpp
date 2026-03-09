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
#include "EventLogger.h"

// States we can transition to
#include "ConfigState.h"
#include "PowerOffState.h"

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
    : dataManager(dataManager), hardwareManager(hardwareManager), timeoutTriggered(false),
    changingState(false)
{}

void CalibrationState::enter(StateManager* manager) {
    LOG_PRINTLN("Entering CalibrationState...");
    EVENT_INFO("Cal:Entered");

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
    if (!changingState) {
        update(manager);
    }   
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
            EVENT_INFO("Cal:Complete");
            manager->changeState(new ConfigState(dataManager, manager->getWebServerManager()));
            changingState = true;
            break;

        case EVENT_CALIBRATION_CANCEL:
            LOG_PRINTLN("[CalibrationState] Event: Calibration Cancelled. Returning to ConfigState.");
            EVENT_WARN("Cal:Cancelled");
            manager->changeState(new ConfigState(dataManager, manager->getWebServerManager()));
            changingState = true;
            break;

        case EVENT_CALIBRATION_FAILED:
            LOG_PRINTLN("[CalibrationState] Event: Calibration Failed. Returning to ConfigState.");
            EVENT_ERROR("Cal:FAILED");
            manager->changeState(new ConfigState(dataManager, manager->getWebServerManager()));
            changingState = true;
            break;
        
        case EVENT_USB_CONNECTED:
            LOG_PRINTLN("[CalibrationState] Event: USB Connected. Changing to PowerOffState.");
            EVENT_WARN("Cal:USB->PwOff");
            manager->changeState(new PowerOffState());
            changingState = true;
            break;
            
        case EVENT_POWER_SWITCH_OFF:
            LOG_PRINTLN("[CalibrationState] Event: Power Switch OFF. Changing to PowerOffState.");
            EVENT_WARN("Cal:PWR->PwOff");
            manager->changeState(new PowerOffState());
            changingState = true;
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
        timeoutTriggered = true;
        LOG_PRINTLN("[CalibrationState] Calibration timeout! No completion within time limit.");
        Event timeoutEvent;
        timeoutEvent.type = EVENT_CALIBRATION_FAILED;
        xQueueSend(manager->getEventQueue(), &timeoutEvent, 0);
    }
}

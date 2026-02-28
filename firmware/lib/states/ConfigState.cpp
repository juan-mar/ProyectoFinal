/****************************************************************
 * @file ConfigState.cpp
 * @brief Implements the ConfigState class logic.
 ****************************************************************/

/****************************************************************
 * Headers
 ****************************************************************/
#include "ConfigState.h"
#include "StateManager.h"
#include "DataManager.h"
#include "TrainingSession.h"
#include "Events.h"
#include "Config.h"
#include "HardwareManager.h"
#include "WebServerManager.h"

// States we can transition to
#include "SyncState.h"
#include "CalibrationState.h"
#include "ManualPlayState.h"
#include "AutoPlayState.h"

/****************************************************************
 * Defines and Constants
 ****************************************************************/
/**
 * @brief The "tick rate" for this state in milliseconds.
 * How often the update() loop will run (e.g., for the web server).
 */
#define CONFIG_STATE_TICK_MS 20

/****************************************************************
 * Class Method Implementations
 ****************************************************************/

ConfigState::ConfigState(DataManager* dataManager, WebServerManager* webServer) 
    : dataManager(dataManager), webServer(webServer),changingState(false) 
{}

void ConfigState::enter(StateManager* manager) {
    LOG_PRINTLN("Entering ConfigState...");
    changingState = false;
    this->sessionConfig = new TrainingSession();
    
    //TODO: Set LEDs as IDLE OFFLINE-CONFIG
    //manager->getUserInterface()->setLedPattern(LED_IDLE_OFFLINE);

    webServer->setTargetSession(this->sessionConfig);
    webServer->begin();
}

void ConfigState::execute(StateManager* manager) {
    Event event;
    QueueHandle_t queue = manager->getEventQueue();

    if (xQueueReceive(queue, &event, CONFIG_STATE_TICK_MS / portTICK_PERIOD_MS) == pdTRUE) {
        handleEvent(manager, event);
    }
    if(!changingState) {
        update(manager);
    }
}

void ConfigState::exit(StateManager* manager) {
    LOG_PRINTLN("Exiting ConfigState...");
    if (this->sessionConfig != nullptr) {
        delete this->sessionConfig;
        this->sessionConfig = nullptr;
        LOG_PRINTLN("ConfigState: Unused session config deleted.");
    }
    webServer->stop();
    webServer->setTargetSession(nullptr);  
}

/****************************************************************
 * Protected Methods
 ****************************************************************/

void ConfigState::handleEvent(StateManager* manager, Event& event) {
    switch (event.type) {
        case EVENT_MODE_ONLINE_ACTIVATED:
            LOG_PRINTLN("[ConfigState] Event: Mode ONLINE. Changing to SyncState.");
            changingState = true;
            manager->changeState(new SyncState(dataManager, manager->getSupabaseClient()));
            break;

        case EVENT_START_CALIBRATION:
            LOG_PRINTLN("[ConfigState] Event: Start Calibration. Changing to CalibrationState.");
            this->sessionConfig = nullptr;
            changingState = true;
            manager->changeState(new CalibrationState(dataManager, manager->getHardwareManager()));
            break;

        case EVENT_START_MANUAL_PLAY:
        {
            LOG_PRINTLN("[ConfigState] Event: Start Manual Play.");
            changingState = true;
            TrainingSession* sessionToPass = this->sessionConfig;            
            this->sessionConfig = nullptr; 
            manager->changeState(new ManualPlayState(dataManager,sessionToPass));
            break;
        }    
        case EVENT_START_AUTO_PLAY:
        {
            LOG_PRINTLN("[ConfigState] Event: Start Auto Play.");
            changingState = true;
            TrainingSession* sessionToPass = this->sessionConfig;            
            this->sessionConfig = nullptr; 
            manager->changeState(new AutoPlayState(dataManager, sessionToPass));
            break;
        }
        default:
            break;
    }
}

void ConfigState::update(StateManager* manager) {
    webServer->update();
}
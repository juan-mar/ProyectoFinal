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
#include "WebServerManager.h"
#include "HardwareManager.h"
#include "EventLogger.h"

// States we can transition to
#include "SyncState.h"
#include "CalibrationState.h"
#include "ManualPlayState.h"
#include "AutoPlayState.h"
#include "PowerOffState.h"

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
    : dataManager(dataManager), webServer(webServer), changingState(false), firstTime(false) 
{}

void ConfigState::enter(StateManager* manager) {
    LOG_PRINTLN("Entering ConfigState...");
    EVENT_INFO("State Config entered");
    PIN_MODE(2, OUTPUT); // Debug LED
    PIN_HIGH(2);          // Turn on debug LED
    changingState = false;
    this->sessionConfig = new TrainingSession();
    
    // SOLUCIÓN 1: Reset WiFi to avoid mode conflicts from previous states
    WiFi.mode(WIFI_OFF);
    vTaskDelay(500 / portTICK_PERIOD_MS);

    webServer->setTargetSession(this->sessionConfig);
    // NOTE: webServer->begin() is deferred to execute() for lazy initialization
    // to avoid stack overflow during state entry. It's called on first execution.
}

void ConfigState::execute(StateManager* manager) {
    // Lazy initialization of web server after queue is stable and task has proper stack
    if (!firstTime) {
        LOG_PRINTLN("ConfigState: Initializing web server...");
        webServer->begin();
        firstTime = true;
        LOG_PRINTLN("ConfigState: Web server started (launcher already active from PowerUpState)");
    }

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
            EVENT_INFO("Config: EVENT_MODE_ONLINE_ACTIVATED");
            changingState = true;
            manager->changeState(new SyncState(dataManager, manager->getSupabaseClient()));
            break;

        case EVENT_START_CALIBRATION:
            LOG_PRINTLN("[ConfigState] Event: Start Calibration. Changing to CalibrationState.");
            EVENT_INFO("Config: EVENT_START_CALIBRATION");
            this->sessionConfig = nullptr;
            changingState = true;
            manager->changeState(new CalibrationState(dataManager, manager->getHardwareManager()));
            break;

        case EVENT_START_MANUAL_PLAY:
        {
            LOG_PRINTLN("[ConfigState] Event: Start Manual Play.");
            EVENT_INFO("Config: EVENT_START_MANUAL_PLAY");
            changingState = true;
            TrainingSession* sessionToPass = this->sessionConfig;            
            this->sessionConfig = nullptr; 
            manager->changeState(new ManualPlayState(dataManager,sessionToPass));
            break;
        }    
        case EVENT_START_AUTO_PLAY:
        {
            LOG_PRINTLN("[ConfigState] Event: Start Auto Play.");
            EVENT_INFO("Config: EVENT_START_AUTO_PLAY");
            changingState = true;
            TrainingSession* sessionToPass = this->sessionConfig;            
            this->sessionConfig = nullptr; 
            manager->changeState(new AutoPlayState(dataManager, sessionToPass));
            break;
        }
        case EVENT_USB_CONNECTED:
            LOG_PRINTLN("[ConfigState] Event: USB Connected. Changing to PowerOffState.");
            EVENT_WARN("Config: USB connected -> PowerOffState");
            changingState = true;
            manager->changeState(new PowerOffState());
            break;
            
        case EVENT_POWER_SWITCH_OFF:
            LOG_PRINTLN("[ConfigState] Event: Power Switch OFF. Changing to PowerOffState.");
            EVENT_WARN("Config: Power OFF -> PowerOffState");
            changingState = true;
            manager->changeState(new PowerOffState());
            break;
            
        default:
            break;
    }
}

void ConfigState::update(StateManager* manager) {
    webServer->update();
}
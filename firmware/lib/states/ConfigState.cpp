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
#include "config.h"

// States we can transition to
#include "SyncState.h"

// #include "ManualPlayState.h"
// #include "AutoPlayState.h"

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

ConfigState::ConfigState(DataManager* dataManager) 
    : dataManager(dataManager) // Constructor stores the pointer
{
    // this->webServer = new WebServerManager(); // se podria crear server aca
}

void ConfigState::enter(StateManager* manager) {
    LOG_PRINTLN("Entering ConfigState...");
    this->sessionConfig = new TrainingSession();

    PIN_LOW(2); // Turn on debug LED to indicate ConfigState
    //TODO:
    // String dogList = dataManager->readDogList();
    // webServer->start(dogList);
}

void ConfigState::execute(StateManager* manager) {
    Event event;
    QueueHandle_t queue = manager->getEventQueue();

    if (xQueueReceive(queue, &event, CONFIG_STATE_TICK_MS / portTICK_PERIOD_MS) == pdTRUE) {
        handleEvent(manager, event);
    }

    update(manager);
}

void ConfigState::exit(StateManager* manager) {
    LOG_PRINTLN("Exiting ConfigState...");
    if (this->sessionConfig != nullptr) {
        delete this->sessionConfig;
        this->sessionConfig = nullptr;
        LOG_PRINTLN("ConfigState: Unused session config deleted.");
    }
    // webServer->stop();
}

/****************************************************************
 * Protected Methods
 ****************************************************************/

void ConfigState::handleEvent(StateManager* manager, Event& event) {
    switch (event.type) {
        case EVENT_MODE_ONLINE_ACTIVATED:
            LOG_PRINTLN("[ConfigState] Event: Mode ONLINE. Changing to SyncState.");
            manager->changeState(new SyncState(dataManager, manager->getSupabaseClient()));
            break;

        case EVENT_START_MANUAL_PLAY:
            LOG_PRINTLN("[ConfigState] Event: Start Manual Play.");
            // manager->changeState(new ManualPlayState(dataManager));
            this->sessionConfig = nullptr;
            break;

        case EVENT_START_AUTO_PLAY:
            LOG_PRINTLN("[ConfigState] Event: Start Auto Play.");
            // manager->changeState(new AutoPlayState(dataManager));
            this->sessionConfig = nullptr;
            break;
        
        default:
            break;
    }
}

void ConfigState::update(StateManager* manager) {
    // Lógica periódica (ej. cada 20ms)
    // webServer->handleClient();
}
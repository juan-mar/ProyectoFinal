/****************************************************************
 * @file ConfigState.cpp
 * @brief Implements the ConfigState class logic.
 ****************************************************************/

 /****************************************************************
 * Headers
 ****************************************************************/
#include "ConfigState.h"
#include "StateManager.h"
//#include "DataManager.h"
#include "Events.h"
#include "Config.h"

// Include headers for states we can transition to
#include "SyncState.h"
// #include "ManualPlayState.h"  
// #include "AutoPlayState.h"   

/****************************************************************
 * Defines
 ****************************************************************/
#define CONFIG_STATE_TICK_MS 20

/****************************************************************
 * Class Method Implementations
 ****************************************************************/
ConfigState::ConfigState(DataManager* dataManager) 
    : dataManager(dataManager)
{
    // this->webServer = new WebServerManager(); // Se peude crear serverWeb ahí 
}

void ConfigState::enter(StateManager* manager) {
    LOG_PRINTLN("Entering ConfigState...");
    
    // String dogList = dataManager->readDogList();
    // webServer->start(dogList);
}

void ConfigState::execute(StateManager* manager) {
    Event event;
    QueueHandle_t queue = manager->getEventQueue();

    // 1. Bloqueo con timeout (para permitir el update periódico)
    if (xQueueReceive(queue, &event, CONFIG_STATE_TICK_MS / portTICK_PERIOD_MS) == pdTRUE) {
        handleEvent(manager, event);
    }

    update(manager);
}

void ConfigState::exit(StateManager* manager) {
    LOG_PRINTLN("Exiting ConfigState...");
    // TODO: Stop and shut down the WebServerManager here
}

void ConfigState::handleEvent(StateManager* manager, Event& event) {
    switch (event.type) {
        case EVENT_MODE_ONLINE_ACTIVATED:
            LOG_PRINTLN("[ConfigState] Event: Mode ONLINE. Changing to SyncState.");
            manager->changeState(new SyncState(dataManager)); // Pasa la dependencia
            break;

        case EVENT_START_MANUAL_PLAY:
            LOG_PRINTLN("[ConfigState] Event: Start Manual Play.");
            // manager->changeState(new ManualPlayState(dataManager));
            break;

        case EVENT_START_AUTO_PLAY:
            LOG_PRINTLN("[ConfigState] Event: Start Auto Play.");
            // manager->changeState(new AutoPlayState(dataManager));
            break;
        
        default:
            break;
    }
}

void ConfigState::update(StateManager* manager) {
    // Lógica periódica
    // webServer->handleClient();
}
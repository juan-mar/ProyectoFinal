/****************************************************************
 * @file StateManager.cpp
 * @brief Implements the methods for the StateManager class.
 ****************************************************************/

/****************************************************************
 * Headers
 ****************************************************************/
#include "StateManager.h" 
#include "State.h"        
#include "Config.h"

// Include the initial state
#include "ConfigState.h"  

/****************************************************************
 * Defines and Constants
 ****************************************************************/
#define EVENT_QUEUE_LENGTH 10

/****************************************************************
 * Class Method Implementations
 ****************************************************************/

StateManager::StateManager(DataManager* dataManager, SupabaseClient* supabaseClient, UserInterface* ui, WebServerManager* _ws) 
                : currentState(nullptr), dataManager(dataManager), supabaseClient(supabaseClient), userInterface(ui),
                ws(_ws)
{
    eventQueue = xQueueCreate(EVENT_QUEUE_LENGTH, sizeof(Event));

    if (eventQueue == NULL) {
        LOG_PRINTLN("FATAL ERROR: Could not create event queue!");
        while(1); // Halt execution
    }

    // Set the initial state, injecting dependencies
    currentState = new ConfigState(this->dataManager, this->ws);
    
    if (currentState != nullptr) {
        currentState->enter(this);
    } else {
        LOG_PRINTLN("FATAL ERROR: Could not create initial state!");
        while(1); // Halt execution
    }
}

StateManager::~StateManager() {
    if (eventQueue != NULL) {
        vQueueDelete(eventQueue);
    }
    if (currentState != nullptr) {
        delete currentState;
    }
}

/**
 * @brief Main execution loop for the state machine.
 */
void StateManager::execute() {
    if (currentState != nullptr) {
        currentState->execute(this);
    }
}

/**
 * @brief Transitions the FSM to a new state.
 */
void StateManager::changeState(State* newState) {
    if (currentState != nullptr) {
        currentState->exit(this);
        delete currentState;
    }

    currentState = newState;

    if (currentState != nullptr) {
        currentState->enter(this);
    } else {
         LOG_PRINTLN("ERROR: Attempted to change to a NULL state!");
    }
}

QueueHandle_t StateManager::getEventQueue() const {
    return eventQueue;
}

DataManager* StateManager::getDataManager() const {
    return dataManager;
}

SupabaseClient* StateManager::getSupabaseClient() const {
    return supabaseClient;
}

UserInterface* StateManager::getUserInterface() const {
    return userInterface;
}

WebServerManager* StateManager::getWebServerManager() const {
    return ws;
}
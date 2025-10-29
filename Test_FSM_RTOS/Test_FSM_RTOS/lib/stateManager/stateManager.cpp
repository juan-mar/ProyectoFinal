/****************************************************************
 * @file StateManager.cpp
 * @brief Implements the methods for the StateManager class.
 ****************************************************************/

/****************************************************************
 * Headers
 ****************************************************************/
#include "StateManager.h" 
#include "State.h"        
#include "ConfigState.h"  
#include <Arduino.h>      

/****************************************************************
 * Defines and Constants
 ****************************************************************/
#define EVENT_QUEUE_LENGTH 10 

/****************************************************************
 * Class Method Implementations
 ****************************************************************/

/**
 * @brief Constructor. Creates the event queue and sets the initial state.
 */
StateManager::StateManager() : currentState(nullptr) {
    // Create the event queue
    eventQueue = xQueueCreate(EVENT_QUEUE_LENGTH, sizeof(Event));

    if (eventQueue == NULL) {
        // Handle critical error
        Serial.println("FATAL ERROR: Could not create event queue!");
        while(1);
    }

    // Set the initial state
    currentState = new ConfigState();
    if (currentState != nullptr) {
        currentState->enter(this);
    } else {
        Serial.println("FATAL ERROR: Could not create initial state!");
        while(1);
    }
}

/**
 * @brief Destructor. Cleans up the queue and current state.
 */
StateManager::~StateManager() {
    // Delete the queue
    if (eventQueue != NULL) {
        vQueueDelete(eventQueue);
    }

    // Delete the current state object to free memory
    if (currentState != nullptr) {
        delete currentState;
        currentState = nullptr;
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
    // 1. Call the exit method on the old state (if it exists)
    if (currentState != nullptr) {
        currentState->exit(this);
        delete currentState; // Free the memory of the old state
    }

    // 2. Assign the new state
    currentState = newState;

    // 3. Call the enter method on the new state (if it's valid)
    if (currentState != nullptr) {
        currentState->enter(this);
    } else {
        Serial.println("ERROR: Attempted to change to a NULL state!");
    }
}

/**
 * @brief Provides read-only access to the event queue handle.
 */
QueueHandle_t StateManager::getEventQueue() const {
    return eventQueue;
}
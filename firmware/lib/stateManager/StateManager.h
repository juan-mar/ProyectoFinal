/****************************************************************
 * @file StateManager.h
 * @brief Declares the StateManager class, which manages the
 * global application state.
 ****************************************************************/

/****************************************************************
 * Include Guards
 ****************************************************************/
#ifndef STATEMANAGER_H
#define STATEMANAGER_H

/****************************************************************
 * Headers
 ****************************************************************/
#include <Arduino.h>
#include <freertos/queue.h> 
#include "Events.h"         
#include "SupabaseClient.h"

/****************************************************************
 * Forward Declarations
 ****************************************************************/
class State; 
class DataManager; // Dependency

/****************************************************************
 * Class Declarations
 ****************************************************************/

/**
 * @brief Manages the state transitions and holds the event queue.
 * This class is the "Context" in the State design pattern.
 */
class StateManager {
public:
    /**
     * @brief Constructor.
     * @param dataManager A pointer to the global DataManager instance.
     */
    StateManager(DataManager* dataManager);

    /**
     * @brief Cleans up the queue and current state.
     */
    ~StateManager();

    /**
     * @brief Main execution loop for the state machine.
     * This method should be called repeatedly by a dedicated task.
     * It delegates the work to the current state's execute() method.
     */
    void execute();

    /**
     * @brief Transitions the FSM to a new state.
     * This involves exiting the old state and entering the new one.
     * @param newState A pointer to the new state object to transition to.
     */
    void changeState(State* newState);

    /**
     * @brief Provides read-only access to the event queue handle.
     * @return The handle to the FreeRTOS event queue.
     */
    QueueHandle_t getEventQueue() const;

    /**
     * @brief Gets the pointer to the DataManager instance.
     * @return A pointer to the DataManager.
     */
    DataManager* getDataManager() const;

    /**
     * @brief Gets the pointer to the SupabaseClient instance.
     * @return A pointer to the SupabaseClient.
     */
    SupabaseClient* getSupabaseClient() const;

private:
    State* currentState;
    QueueHandle_t eventQueue;
    DataManager* dataManager;
    SupabaseClient* supabaseClient;
};

#endif // STATEMANAGER_H

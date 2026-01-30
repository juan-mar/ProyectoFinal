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


/****************************************************************
 * Forward Declarations
 ****************************************************************/
class State; 
class DataManager; // Dependency
class HardwareManager; // Dependency
class SupabaseClient; // Dependency
class WebServerManager; // Dependency

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
    StateManager(DataManager* dataManager, SupabaseClient* supabaseClient, 
                HardwareManager* hw, WebServerManager* ws);

    /**
     * @brief Cleans up the queue and current state.
     */
    ~StateManager();
    
    /**
     * @brief Initializes the StateManager and set the first state stateConfig.
     */
    void begin();

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

    QueueHandle_t getEventQueue() const;
    DataManager* getDataManager() const;
    SupabaseClient* getSupabaseClient() const;
    HardwareManager* getHardwareManager() const;
    WebServerManager* getWebServerManager() const;

private:
    State* currentState;
    QueueHandle_t eventQueue;
    DataManager* dataManager;
    SupabaseClient* supabaseClient;
    HardwareManager* hw;
    WebServerManager* ws;
};

#endif // STATEMANAGER_H

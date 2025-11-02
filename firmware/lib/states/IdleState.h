/****************************************************************
 * @file IdleState.h
 * @brief Declares the IdleState class (low-power sleep).
 ****************************************************************/

/****************************************************************
 * Include Guards
 ****************************************************************/
#ifndef IDLE_STATE_H
#define IDLE_STATE_H

/****************************************************************
 * Headers
 ****************************************************************/
#include "State.h"

/****************************************************************
 * Forward Declarations
 ****************************************************************/
class DataManager;

/****************************************************************
 * Class Declarations
 ****************************************************************/

/**
 * @brief Active after a successful sync (ONLINE mode).
 * Puts the device into light sleep to save power, waking
 * only on hardware interrupts (like the mode switch).
 */
class IdleState : public State {
public:
    /**
     * @brief Constructor that receives dependencies.
     * @param dataManager Pointer to the global DataManager.
     */
    IdleState(DataManager* dataManager);

    virtual void enter(StateManager* manager) override;
    virtual void execute(StateManager* manager) override;
    virtual void exit(StateManager* manager) override;

protected:
    virtual void handleEvent(StateManager* manager, Event& event) override;
    virtual void update(StateManager* manager) override;

private:
    DataManager* dataManager; // Pointer to the injected dependency
};

#endif // IDLE_STATE_H
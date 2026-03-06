/****************************************************************
 * @file PowerUpState.h
 * @brief Declares the PowerUpState class.
 * 
 * This state is entered when the device wakes up from IdleState
 * with OFFLINE mode selected, or during initial boot.
 * 
 * PowerUpState activates the launcher and waits for stabilization
 * before transitioning to ConfigState, providing clean power
 * management and avoiding brownout conflicts with WiFi.
 ****************************************************************/

#ifndef POWER_UP_STATE_H
#define POWER_UP_STATE_H

#include <Arduino.h>
#include "State.h"

class PowerUpState : public State {
public:
    /**
     * @brief Constructor for PowerUpState.
     */
    PowerUpState();

    virtual void enter(StateManager* manager) override;
    virtual void execute(StateManager* manager) override;
    virtual void exit(StateManager* manager) override;

protected:
    virtual void handleEvent(StateManager* manager, Event& event) override;
    virtual void update(StateManager* manager) override;

private:
    uint32_t stateStartTime;  // Track when we entered this state
};

#endif // POWER_UP_STATE_H

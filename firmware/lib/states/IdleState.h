/****************************************************************
 * @file IdleState.h
 * @brief Declares the IdleState class.
 * This state puts the device into light sleep to save power
 * and wakes up on hardware events (like the mode switch).
 ****************************************************************/

#ifndef IDLE_STATE_H
#define IDLE_STATE_H

#include "State.h"

class IdleState : public State {
public:
    virtual void enter(StateManager* manager) override;
    virtual void execute(StateManager* manager) override;
    virtual void exit(StateManager* manager) override;
};

#endif // IDLE_STATE_H
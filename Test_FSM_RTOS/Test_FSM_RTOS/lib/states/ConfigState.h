/****************************************************************
 * @file ConfigState.h
 * @brief Declares the ConfigState class.
 * This state is active in OFFLINE mode, running the web server.
 ****************************************************************/

#ifndef CONFIG_STATE_H
#define CONFIG_STATE_H

#include "State.h"

class ConfigState : public State {
public:
    virtual void enter(StateManager* manager) override;
    virtual void execute(StateManager* manager) override;
    virtual void exit(StateManager* manager) override;
};

#endif // CONFIG_STATE_H
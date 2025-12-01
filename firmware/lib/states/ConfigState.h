/****************************************************************
 * @file ConfigState.h
 * @brief Declares the ConfigState class (Offline web server).
 ****************************************************************/

/****************************************************************
 * Include Guards
 ****************************************************************/
#ifndef CONFIG_STATE_H
#define CONFIG_STATE_H

/****************************************************************
 * Headers
 ****************************************************************/
#include "State.h"

/****************************************************************
 * Forward Declarations
 ****************************************************************/
class DataManager;
class TrainingSession;
class WebServerManager; // Futuro módulo

/****************************************************************
 * Class Declarations
 ****************************************************************/

/**
 * @brief Active in OFFLINE mode, running the web server for config.
 */
class ConfigState : public State {
public:
    ConfigState(DataManager* dataManager, WebServerManager* webServer);

    virtual void enter(StateManager* manager) override;
    virtual void execute(StateManager* manager) override;
    virtual void exit(StateManager* manager) override;

protected:
    virtual void handleEvent(StateManager* manager, Event& event) override;
    virtual void update(StateManager* manager) override;

private:
    DataManager* dataManager;       // Pointer to the injected dependency
    TrainingSession* sessionConfig; // Pointer to the injected dependency
    WebServerManager* webServer; // Futuro puntero
};

#endif // CONFIG_STATE_H
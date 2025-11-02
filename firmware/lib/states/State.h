/****************************************************************
 * @file State.h
 * @brief Defines the abstract base class (interface) for all
 * application states.
 ****************************************************************/

#ifndef STATE_H
#define STATE_H

/****************************************************************
 * Headers
 ****************************************************************/
#include "Events.h"

/****************************************************************
 * Forward Declarations
 ****************************************************************/
class StateManager;

/****************************************************************
 * Class Declarations
 ****************************************************************/
class State {
public:
    virtual ~State() {}

    /**
     * @brief Called ONCE when transitioning INTO this state.
     */
    virtual void enter(StateManager* manager) = 0;

    /**
     * @brief Called REPEATEDLY by the StateManager task.
     * This method is responsible for ALL logic, including
     * event handling, periodic updates, AND blocking (delaying).
     */
    virtual void execute(StateManager* manager) = 0;

    /**
     * @brief Called ONCE when transitioning OUT of this state.
     */
    virtual void exit(StateManager* manager) = 0;

protected:
    /**
     * @brief Processes a single event from the queue.
     */
    virtual void handleEvent(StateManager* manager, Event& event) = 0;

    /**
     * @brief Runs periodic logic for the state.
     */
    virtual void update(StateManager* manager) = 0;
};
#endif // STATE_H
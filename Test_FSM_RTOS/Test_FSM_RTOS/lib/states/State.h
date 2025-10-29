/****************************************************************
 * @file State.h
 * @brief Defines the abstract base class (interface) for all
 * application states.
 ****************************************************************/

/****************************************************************
 * Include Guards
 ****************************************************************/
#ifndef STATE_H
#define STATE_H

/****************************************************************
 * Headers
 ****************************************************************/


/****************************************************************
 * Forward Declarations
 ****************************************************************/
class StateManager; 

/****************************************************************
 * Class Declarations
 ****************************************************************/

/**
 * @brief Abstract base class for all states in the FSM.
 */
class State {
public:
    /**
     * @brief Virtual destructor.
     * Ensures that the destructor of the child class is called
     * when we 'delete' a pointer to the base class.
     */
    virtual ~State() {}

    /**
     * @brief Called ONCE when transitioning INTO this state.
     * Use this to initialize hardware, start tasks, etc.
     * @param manager Pointer to the StateManager (context).
     */
    virtual void enter(StateManager* manager) = 0;

    /**
     * @brief Called REPEATEDLY in the main FSM task loop.
     * This is where the state's main logic (like checking
     * for events) goes.
     * @param manager Pointer to the StateManager (context).
     */
    virtual void execute(StateManager* manager) = 0;

    /**
     * @brief Called ONCE when transitioning OUT of this state.
     * Use this to de-initialize hardware, stop servers, etc.
     * @param manager Pointer to the StateManager (context).
     */
    virtual void exit(StateManager* manager) = 0;
};

#endif // STATE_H
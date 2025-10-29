/****************************************************************
 * @file SyncState.h
 * @brief Declares the SyncState class.
 * This state is active in ONLINE mode, handling data upload.
 ****************************************************************/

#ifndef SYNC_STATE_H
#define SYNC_STATE_H

/****************************************************************
 * Headers
 ****************************************************************/
#include "State.h"

/****************************************************************
 * Classes / Functions prototypes
 ****************************************************************/
class SyncState : public State {
public:
    virtual void enter(StateManager* manager) override;
    virtual void execute(StateManager* manager) override;
    virtual void exit(StateManager* manager) override;
};

#endif // SYNC_STATE_H
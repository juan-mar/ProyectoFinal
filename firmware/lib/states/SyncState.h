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
 * Forward Declarations
 ****************************************************************/
class DataManager;

/****************************************************************
 * Classes / Functions prototypes
 ****************************************************************/
/**
 * @brief Active in ONLINE mode, handles WiFi connection
 * and data synchronization with Supabase.
 */
class SyncState : public State {
public:
    /**
     * @brief Constructor that receives dependencies.
     * @param dataManager Pointer to the global DataManager.
     */
    SyncState(DataManager* dataManager);

    virtual void enter(StateManager* manager) override;
    virtual void execute(StateManager* manager) override;
    virtual void exit(StateManager* manager) override;

protected:
    virtual void handleEvent(StateManager* manager, Event& event) override;
    virtual void update(StateManager* manager) override;

private:
    DataManager* dataManager; // Pointer to the injected dependency
    
    /**
     * @brief Private helper to start the sync process.
     * Called from enter() or on a WiFi connected event.
     */
    void beginSynchronization(StateManager* manager);
};

#endif // SYNC_STATE_H
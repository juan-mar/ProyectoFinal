/****************************************************************
 * @file    SyncState.h
 * @brief   Declares the SyncState class (Online data sync).
 ****************************************************************/

/****************************************************************
 * Include Guards
 ****************************************************************/
#ifndef SYNC_STATE_H
#define SYNC_STATE_H

/****************************************************************
 * Headers
 ****************************************************************/
#include "State.h"
#include <freertos/task.h> // Para TaskHandle_t

/****************************************************************
 * Forward Declarations
 ****************************************************************/
class DataManager;
class SupabaseClient;

/****************************************************************
 * Class Declarations
 ****************************************************************/

/**
 * @brief Active in ONLINE mode, handles WiFi connection
 * and data synchronization with Supabase via a background task.
 */
class SyncState : public State {
public:
    /**
     * @brief Constructor that receives dependencies.
     * @param dataManager Pointer to the global DataManager.
     * @param supabaseClient Pointer to the global SupabaseClient.
     */
    SyncState(DataManager* dataManager, SupabaseClient* supabaseClient);

    virtual void enter(StateManager* manager) override;
    virtual void execute(StateManager* manager) override;
    virtual void exit(StateManager* manager) override;

protected:
    virtual void handleEvent(StateManager* manager, Event& event) override;
    virtual void update(StateManager* manager) override;

private:
    DataManager* dataManager;
    SupabaseClient* supabaseClient;
    
    /**
     * @brief Handle to the background sync task.
     */
    TaskHandle_t h_syncTask;
};

#endif // SYNC_STATE_H
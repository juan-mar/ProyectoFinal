/****************************************************************
 * @file ManualPlayState.h
 * @brief Handles the manual game logic.
 * Listens for remote control events via RemoteControl driver
 * and logs training sessions via DataManager.
 ****************************************************************/

#ifndef MANUAL_PLAY_STATE_H
#define MANUAL_PLAY_STATE_H

#include "State.h"

// Forward Declarations
class DataManager;
class TrainingSession;

class ManualPlayState : public State {
public:
    /**
     * @brief Constructor.
     * @param dm Pointer to DataManager (storage).
     * @param rc Pointer to RemoteControl (hardware driver).
     * @param session Pointer to the active TrainingSession object (takes ownership).
     */
    ManualPlayState(DataManager* dm, TrainingSession* session);

    virtual void enter(StateManager* manager) override;
    virtual void execute(StateManager* manager) override;
    virtual void exit(StateManager* manager) override;

protected:
    virtual void handleEvent(StateManager* manager, Event& event) override;
    virtual void update(StateManager* manager) override;

private:
    DataManager* dataManager;
    TrainingSession* currentSession;
    bool changingState; // Escudo para evitar manejar eventos durante la transición
    unsigned long roundStartMillis; // Para medir duración del juego
    bool isWaitingReward; // Flag para controlar el temporizador de recompensa
    int rewardDelayMs; // Tiempo a esperar antes de auto-disparar el éxito
    int rewardStartTime;

    
    // Helper para guardar y limpiar
    void saveRun(const char* result);
};

#endif // MANUAL_PLAY_STATE_H
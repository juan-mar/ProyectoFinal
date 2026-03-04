/****************************************************************
 * @file AutoPlayState.h
 * @brief Handles the automatic game logic (Single Shot).
 ****************************************************************/

#ifndef AUTO_PLAY_STATE_H
#define AUTO_PLAY_STATE_H

#include "State.h"

class DataManager;
class TrainingSession;

class AutoPlayState : public State {
public:
    AutoPlayState(DataManager* dm, TrainingSession* session);

    virtual void enter(StateManager* manager) override;
    virtual void execute(StateManager* manager) override;
    virtual void exit(StateManager* manager) override;

protected:
    virtual void handleEvent(StateManager* manager, Event& event) override;
    virtual void update(StateManager* manager) override;

private:
    DataManager* dataManager;
    TrainingSession* currentSession;
    bool changingState;  

    enum AutoSubState {
        TIMEOUT_FAIL,
        WAITING_FOR_DOG,            // Esperando a que el collar entre en rango
        DOG_DETECTED_TIMING,        // Perro en rango, contando el tiempo exigido
        DISPENSING_REWARD           // Solenoide disparado, esperando confirmación
    };
    
    AutoSubState internalState;
    
    unsigned long stateStartTime;     // Cuándo empezó el juego (para el Timeout global)
    unsigned long detectionStartTime; // Cuándo el perro entró a la zona

    void saveRun(StateManager* manager, const char* result);
};

#endif // AUTO_PLAY_STATE_H
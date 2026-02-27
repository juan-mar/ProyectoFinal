#include "AutoPlayState.h"
#include "SyncState.h"
#include "StateManager.h"
#include "DataManager.h"
#include "TrainingSession.h"
#include "HardwareManager.h"
#include "ConfigState.h"
#include "Events.h"
#include "Config.h"

AutoPlayState::AutoPlayState(DataManager* dm, TrainingSession* session)
    : dataManager(dm), currentSession(session), internalState(WAITING_FOR_DOG),
    changingState(false)

{}

void AutoPlayState::enter(StateManager* manager) {
    LOG_PRINTLN("Entering AutoPlayState (Single Shot Mode)...");
    changingState = false;

    // Iniciar cronómetro global del juego
    stateStartTime = millis();
    vTaskDelay(pdMS_TO_TICKS(100)); 

    // Encender BLE
    manager->getHardwareManager()->sendCommand(CMD_TAG_POWER_ON, CMD_TAG_PARAM_DETECTION);
    manager->getHardwareManager()->sendCommand(CMD_LAUNCHER_ON, 0);
    
    vTaskDelay(pdMS_TO_TICKS(50)); 
}

void AutoPlayState::execute(StateManager* manager) {
    Event event;
    
    if (xQueueReceive(manager->getEventQueue(), &event, pdMS_TO_TICKS(50)) == pdTRUE) {
        handleEvent(manager, event);
    }
    if (!changingState) {
        update(manager);
    }   
}

void AutoPlayState::exit(StateManager* manager) {
    LOG_PRINTLN("Exiting AutoPlayState...");
    manager->getHardwareManager()->sendCommand(CMD_TAG_POWER_OFF);
    manager->getHardwareManager()->sendCommand(CMD_LAUNCHER_OFF, 0);
    vTaskDelay(pdMS_TO_TICKS(500));
    if (currentSession != nullptr) {
        delete currentSession;
        currentSession = nullptr;
    }
}

void AutoPlayState::handleEvent(StateManager* manager, Event& event) {
    switch (event.type) {
        case EVENT_DOG_DETECTED:
            if (internalState == WAITING_FOR_DOG) {
                LOG_PRINTLN("[Auto] ¡Perro detectado! Iniciando exigencia...");
                internalState = DOG_DETECTED_TIMING;
                detectionStartTime = millis();
            }
            break;

        case EVENT_DOG_LOST:
            if (internalState == DOG_DETECTED_TIMING) {
                LOG_PRINTLN("[Auto] Perro salió antes de tiempo. Reiniciando espera...");
                internalState = WAITING_FOR_DOG;
            }
            break;

        case EVENT_TRAINING_SUCCESS:
            if (internalState == DISPENSING_REWARD) {
                LOG_PRINTLN("[Auto] Reward Dispensed! Game Over (Win).");
                manager->getHardwareManager()->sendCommand(CMD_SOLENOID_FIRE);
                saveRun("success");
                changingState = true;
                manager->changeState(new ConfigState(dataManager, manager->getWebServerManager()));
            }
            break;

        case EVENT_TRAINING_FAILED:
            if (internalState == TIMEOUT_FAIL) {
                LOG_PRINTLN("[Auto] TIMEOUT FAIL. Game Over (Error).");
                saveRun("fail");
                changingState = true;
                manager->changeState(new ConfigState(dataManager, manager->getWebServerManager()));
            }
            break;

        case EVENT_MODE_ONLINE_ACTIVATED:
            LOG_PRINTLN("[AutoPlay] Event: Mode ONLINE. Changing to SyncState.");
            changingState = true;
            manager->changeState(new SyncState(dataManager, manager->getSupabaseClient()));
            break;

        default:
            break;
    }
}

void AutoPlayState::update(StateManager* manager) {
    unsigned long now = millis();

    switch (internalState) {
        case WAITING_FOR_DOG:
        {
            unsigned long maxWaitMs = currentSession->getTimeout() * 1000UL; // a ms 
            if (now - stateStartTime >= maxWaitMs) {
                LOG_PRINTLN("[Auto] TIMEOUT. El perro nunca vino o se aburrió.");
                
                // Volvemos a levantar el portal web para que el usuario recargue
                Event ev;
                ev.type = EVENT_TRAINING_FAILED; // Usamos este evento para indicar que el juego terminó sin éxito
                xQueueSend(manager->getEventQueue(), &ev, 0);
                internalState = TIMEOUT_FAIL; // Reiniciamos el estado interno por si acaso
            }
            break;
        }

        case DOG_DETECTED_TIMING:
        {
            unsigned long requiredTimeMs = currentSession->getDuration() * 1000;
            if (now - detectionStartTime >= requiredTimeMs) {
                LOG_PRINTLN("[Auto] ¡Tiempo cumplido! Disparando premio...");
                internalState = DISPENSING_REWARD;
                Event ev;
                ev.type = EVENT_TRAINING_SUCCESS; // Usamos este evento para indicar que el juego terminó con éxito
                xQueueSend(manager->getEventQueue(), &ev, 0);
            }
            break;
        }
        
        default:
            break;
    }
}

void AutoPlayState::saveRun(const char* result) {
    if (currentSession == nullptr) return;
    
    currentSession->setResult(result);
    currentSession->setDeviceCode(dataManager->getDeviceID());
      
    String json;
    if (currentSession->serialize(json)) {
        dataManager->saveSessionFile(json);
        LOG_PRINTF(">> Run saved with result: %s\n", result);
    }
}
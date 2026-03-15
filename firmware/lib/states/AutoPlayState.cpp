#include "AutoPlayState.h"
#include "SyncState.h"
#include "StateManager.h"
#include "DataManager.h"
#include "TrainingSession.h"
#include "HardwareManager.h"
#include "ConfigState.h"
#include "Events.h"
#include "Config.h"
#include "EventLogger.h"
#include "PowerOffState.h"

AutoPlayState::AutoPlayState(DataManager* dm, TrainingSession* session)
    : dataManager(dm), currentSession(session), internalState(WAITING_FOR_DOG),
    changingState(false)

{}

void AutoPlayState::enter(StateManager* manager) {
    LOG_PRINTLN("Entering AutoPlayState (Single Shot Mode)...");
    EVENT_INFO("Aut:Entered");
    changingState = false;

    // Iniciar cronómetro global del juego
    stateStartTime = millis();
    LOG_PRINTLN("[AutoPlay] Iniciando juego automático. Tiempo límite: " + String(currentSession->getTimeout()) + " segundos.");
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
                EVENT_INFO("Aut:Dog Detected");
                internalState = DOG_DETECTED_TIMING;
                detectionStartTime = millis();
            }
            break;

        case EVENT_DOG_LOST:
            if (internalState == DOG_DETECTED_TIMING) {
                LOG_PRINTLN("[Auto] Perro salió antes de tiempo. Reiniciando espera...");
                EVENT_WARN("Aut:Dog Lost!");
                internalState = WAITING_FOR_DOG;
                stateStartTime = millis(); 
            }
            break;

        case EVENT_TRAINING_SUCCESS:
            if (internalState == DISPENSING_REWARD) {
                LOG_PRINTLN("[Auto] Reward: Saving session data BEFORE firing...");
                EVENT_INFO("Aut:Success");
                // CRITICAL: Save session to flash BEFORE firing to prevent data loss from electrical noise
                saveRun(manager, "success");
                // Now safe to fire (data already persisted)
                manager->getHardwareManager()->sendCommand(CMD_SOLENOID_FIRE);
                LOG_PRINTLN("[Auto] Reward Dispensed! Game Over (Win).");
                changingState = true;
                manager->changeState(new ConfigState(dataManager, manager->getWebServerManager()));
            }
            break;

        case EVENT_TRAINING_FAILED:
            if (internalState == TIMEOUT_FAIL) {
                LOG_PRINTLN("[Auto] TIMEOUT FAIL. Game Over (Error).");
                EVENT_ERROR("Aut:Timeout FAIL");
                saveRun(manager, "fail");
                changingState = true;
                manager->changeState(new ConfigState(dataManager, manager->getWebServerManager()));
            }
            break;

        case EVENT_MODE_ONLINE_ACTIVATED:
            LOG_PRINTLN("[AutoPlay] Event: Mode ONLINE. Changing to SyncState.");
            EVENT_INFO("Aut:->Sync");
            changingState = true;
            manager->changeState(new SyncState(dataManager, manager->getSupabaseClient()));
            break;
            
        case EVENT_USB_CONNECTED:
            LOG_PRINTLN("[AutoPlay] Event: USB Connected. Changing to PowerOffState.");
            EVENT_WARN("Aut:USB->PwOff");
            changingState = true;
            manager->changeState(new PowerOffState());
            break;
            
        case EVENT_POWER_SWITCH_OFF:
            LOG_PRINTLN("[AutoPlay] Event: Power Switch OFF. Changing to PowerOffState.");
            EVENT_WARN("Aut:PWR->PwOff");
            changingState = true;
            manager->changeState(new PowerOffState());
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

void AutoPlayState::saveRun(StateManager* manager, const char* result) {
    if (currentSession == nullptr) return;
    
    currentSession->setResult(result);
    currentSession->setDeviceCode(dataManager->getDeviceID());
    
    // Obtener datos ambientales del sensor
    EnvData envData = manager->getHardwareManager()->getEnvironmentData();
    
    if (envData.valid) {
        // Crear JSON string con los datos ambientales
        String conditionsJson = "{\"temp\":" + String(envData.temperature, 1) + 
                                ",\"humidity\":" + String(envData.humidity, 1) + 
                                ",\"pressure\":" + String(envData.pressure, 1) + "}";
        currentSession->setConditions(conditionsJson);
        LOG_PRINTF("[Auto] Condiciones ambientales: %s\n", conditionsJson.c_str());
    } else {
        LOG_PRINTLN("[Auto] WARNING: Sensor ambiental no válido");
    }
      
    String json;
    if (currentSession->serialize(json)) {
        dataManager->saveSessionToChunk(json, currentSession->getStartedAt());
        LOG_PRINTF(">> Run saved with result: %s\n", result);
    }
}
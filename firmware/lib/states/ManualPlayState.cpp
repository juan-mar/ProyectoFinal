/****************************************************************
 * @file ManualPlayState.cpp
 * @brief Implements ManualPlayState logic.
 ****************************************************************/

#include "ManualPlayState.h"
#include "StateManager.h"
#include "DataManager.h"
#include "TrainingSession.h"
#include "HardwareManager.h"
#include "ConfigState.h"
#include "Events.h"
#include "config.h"
#include "SyncState.h"

// Constructor actualizado con RemoteControl
ManualPlayState::ManualPlayState(DataManager* dm, TrainingSession* session)
    : dataManager(dm), currentSession(session),changingState(false), 
    roundStartMillis(0), isWaitingReward(false), rewardDelayMs(0), rewardStartTime(0)
    
{
}

void ManualPlayState::enter(StateManager* manager) {
    LOG_PRINTLN("Entering ManualPlayState (Multi-Shot Mode)...");
    changingState = false; 

    // 1. Inicializar HW del control remoto
    manager->getHardwareManager()->sendCommand(CMD_REMOTE_POWER_ON);
    
    // 2. Esperar estabilización
    vTaskDelay(50 / portTICK_PERIOD_MS);
    roundStartMillis = millis();
    isWaitingReward = false;
    rewardDelayMs = 0;
    rewardStartTime = 0;
}



void ManualPlayState::execute(StateManager* manager) {
    Event event;
    
    if (xQueueReceive(manager->getEventQueue(), &event, pdMS_TO_TICKS(50)) == pdTRUE) {
        handleEvent(manager, event);
    }
    if (!changingState) {
        update(manager);
    } 
}

void ManualPlayState::exit(StateManager* manager) {
    LOG_PRINTLN("Exiting ManualPlayState...");

    // 1. Detener Software y HW del control remoto
    manager->getHardwareManager()->sendCommand(CMD_REMOTE_POWER_OFF);
    
    if (currentSession != nullptr) {
        delete currentSession;
        currentSession = nullptr;
    }
}
void ManualPlayState::handleEvent(StateManager* manager, Event& event) {
    if (changingState) return;

    switch (event.type) {
        
        // --- EVENTO 1: DETECCIÓN (Control Remoto: Botón "BIEN") ---
        case EVENT_DOG_DETECTED:
            LOG_PRINTLN("[Manual] Botón BIEN (Detectado). Iniciando cuenta regresiva...");
            isWaitingReward = true;
            rewardStartTime = millis();
            rewardDelayMs = currentSession->getDuration() * 1000; 
            break;

        // --- EVENTO 2: PÉRDIDA (Control Remoto: Botón "MAL") ---
        case EVENT_DOG_LOST:
            if (isWaitingReward) {
                // El perro rompió la posición antes de tiempo
                LOG_PRINTLN("[Manual] Botón MAL. Perro rompió posición. Cancelando temporizador.");
                isWaitingReward = false; 
            } else {
                // Marcó la caja equivocada directamente
                LOG_PRINTLN("[Manual] Botón MAL sin posición. Anotando fallo definitivo...");
                
                // Auto-disparamos el evento de fallo
                Event ev;
                ev.type = EVENT_TRAINING_FAILED;
                xQueueSend(manager->getEventQueue(), &ev, 0);
            }
            break;

        // --- EVENTO 3: ÉXITO (Auto-disparado por el update) ---
        case EVENT_TRAINING_SUCCESS:
            LOG_PRINTLN("[Manual] Procesando ÉXITO. Disparando premio...");
            manager->getHardwareManager()->sendCommand(CMD_SOLENOID_FIRE, 0);
            saveRun("success");
            break;

        // --- EVENTO 4: FALLO (Auto-disparado por el Botón "MAL") ---
        case EVENT_TRAINING_FAILED:
            LOG_PRINTLN("[Manual] Procesando FALLO...");
            saveRun("fail");
            break;

        // --- EVENTO 5: FIN DEL JUEGO (Control Remoto: Botón 3 o Mantener presionado) ---
        case EVENT_PLAY_FINISHED:
            LOG_PRINTLN("[Manual] Fin del entrenamiento. Saliendo a Config...");
            changingState = true;
            manager->changeState(new ConfigState(dataManager, manager->getWebServerManager()));
            break;

        case EVENT_MODE_ONLINE_ACTIVATED:
            LOG_PRINTLN("[ConfigState] Event: Mode ONLINE. Changing to SyncState.");
            changingState = true;
            manager->changeState(new SyncState(dataManager, manager->getSupabaseClient()));
            break;

        default:
            break;
    }
}

void ManualPlayState::update(StateManager* manager) {
    if (isWaitingReward) {
        if (millis() - rewardStartTime >= rewardDelayMs) {
            
            LOG_PRINTLN("[Manual] ¡Tiempo cumplido! Auto-disparando EVENT_TRAINING_SUCCESS...");
            isWaitingReward = false; // Apagamos el temporizador
            
            // Creamos un evento de éxito y lo inyectamos en nuestra propia cola
            Event ev;
            ev.type = EVENT_TRAINING_SUCCESS;
            xQueueSend(manager->getEventQueue(), &ev, 0); 
        }
    }
}

void ManualPlayState::saveRun(const char* result) {
    if (currentSession == nullptr) return;

    currentSession->setResult(result);    
    
    // 2. Serializar y Guardar en LittleFS
    String json;
    if (currentSession->serialize(json)) {
        if (dataManager->saveSessionFile(json)) {
            LOG_PRINTF(">> Tiro Manual guardado: %s \n", result);
        }
    }

    // 3. LIMPIAR PARA EL SIGUIENTE DISPARO
    currentSession->setResult("");
    
    //Adelantamos el reloj base sumando la duración de esta ronda
    unsigned long now = millis();
    int elapsedSeconds = (now - roundStartMillis) / 1000;
    currentSession->addSecondsToTimeStamp(elapsedSeconds);
    
    roundStartMillis = millis();
    
    LOG_PRINTLN("[Manual] Reloj avanzado. Listo para el próximo tiro...");
}
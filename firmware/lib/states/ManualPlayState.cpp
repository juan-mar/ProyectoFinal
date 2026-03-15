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
#include "EventLogger.h"
#include "SyncState.h"
#include "PowerOffState.h"

#define MIN_MANUAL_SHOT_INTERVAL_S 40

// Constructor actualizado con RemoteControl
ManualPlayState::ManualPlayState(DataManager* dm, TrainingSession* session)
    : dataManager(dm), currentSession(session),changingState(false), 
    roundStartMillis(0), isWaitingReward(false), rewardDelayMs(0), rewardStartTime(0)
    
{
}

void ManualPlayState::enter(StateManager* manager) {
    unsigned long enterTime = millis();
    LOG_PRINTF("[Manual][t=%lu] ===== ENTERING ManualPlayState (Multi-Shot Mode) =====\n", enterTime);
    EVENT_INFO("Man:Entered");
    changingState = false; 

    // 1. Inicializar HW del control remoto
    LOG_PRINTF("[Manual][t=%lu] Enviando comando CMD_REMOTE_POWER_ON...\n", millis());
    manager->getHardwareManager()->sendCommand(CMD_REMOTE_POWER_ON);
    manager->getHardwareManager()->sendCommand(CMD_MSG_SET,USER_MSG_IDLE);
    

    // 2. Esperar estabilización
    vTaskDelay(50 / portTICK_PERIOD_MS);
    roundStartMillis = millis();
    LOG_PRINTF("[Manual][t=%lu] roundStartMillis inicializado = %lu\n", millis(), roundStartMillis);
    
    isWaitingReward = false;
    rewardDelayMs = 0;
    rewardStartTime = 0;
    LOG_PRINTF("[Manual][t=%lu] Estado inicial: isWaitingReward=false\n", millis());
}



void ManualPlayState::execute(StateManager* manager) {
    Event event;
    unsigned long execTime = millis();
    
    if (xQueueReceive(manager->getEventQueue(), &event, pdMS_TO_TICKS(50)) == pdTRUE) {
        LOG_PRINTF("[Manual][t=%lu] >>> EVENTO RECIBIDO: type=%d <<<\n", execTime, event.type);
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
        {
            unsigned long eventTime = millis();
            rewardDelayMs = currentSession->getDuration() * 1000;
            LOG_PRINTF("[Manual][t=%lu] EVENT_DOG_DETECTED: Iniciando cuenta regresiva de %d ms\n", 
                       eventTime, rewardDelayMs);
            isWaitingReward = true;
            rewardStartTime = eventTime;
            LOG_PRINTF("[Manual][t=%lu] rewardStartTime = %lu, targetTime = %lu\n", 
                       eventTime, rewardStartTime, rewardStartTime + rewardDelayMs);
            break;
        }

        // --- EVENTO 2: PÉRDIDA (Control Remoto: Botón "MAL") ---
        case EVENT_DOG_LOST:
        {
            unsigned long eventTime = millis();
            if (isWaitingReward) {
                // El perro rompió la posición antes de tiempo
                unsigned long elapsedMs = eventTime - rewardStartTime;
                LOG_PRINTF("[Manual][t=%lu] EVENT_DOG_LOST: Perro rompió posición después de %lu ms (necesitaba %d ms)\n",
                           eventTime, elapsedMs, rewardDelayMs);
                EVENT_INFO("Man:Dog Lost RWD");
                isWaitingReward = false; 
            } else {
                // Marcó la caja equivocada directamente
                LOG_PRINTF("[Manual][t=%lu] EVENT_DOG_LOST: Fallo directo (sin posición activa)\n", eventTime);
                
                // Auto-disparamos el evento de fallo
                Event ev;
                ev.type = EVENT_TRAINING_FAILED;
                xQueueSend(manager->getEventQueue(), &ev, 0);
            }
            break;
        }

        // --- EVENTO 3: ÉXITO (Auto-disparado por el update) ---
        case EVENT_TRAINING_SUCCESS:
        {
            unsigned long eventTime = millis();
            LOG_PRINTF("[Manual][t=%lu] EVENT_TRAINING_SUCCESS: Guardando datos ANTES de disparar...\n", eventTime);
            EVENT_INFO("Man:Success");
            // CRITICAL: Save session to flash BEFORE firing to prevent data loss from electrical noise
            saveRun(manager, "success");
            // Now safe to fire (data already persisted)
            manager->getHardwareManager()->sendCommand(CMD_SOLENOID_FIRE, 0);
            break;
        }

        // --- EVENTO 4: FALLO (Auto-disparado por el Botón "MAL") ---
        case EVENT_TRAINING_FAILED:
        {
            unsigned long eventTime = millis();
            LOG_PRINTF("[Manual][t=%lu] EVENT_TRAINING_FAILED: Guardando fallo...\n", eventTime);
            EVENT_INFO("Man:Failed");
            saveRun(manager, "fail");
            break;
        }

        // --- EVENTO 5: FIN DEL JUEGO (Control Remoto: Doble BTN1 o botón FIN) ---
        case EVENT_PLAY_FINISHED:
        {
            unsigned long eventTime = millis();
            LOG_PRINTF("[Manual][t=%lu] EVENT_PLAY_FINISHED: Finalizando entrenamiento...\n", eventTime);
            changingState = true;
            manager->changeState(new ConfigState(dataManager, manager->getWebServerManager()));
            break;
        }

        case EVENT_MODE_ONLINE_ACTIVATED:
            LOG_PRINTLN("[Manual] Event: Mode ONLINE. Changing to SyncState.");
            EVENT_INFO("Man:->Sync");
            changingState = true;
            manager->changeState(new SyncState(dataManager, manager->getSupabaseClient()));
            break;
            
        case EVENT_USB_CONNECTED:
            LOG_PRINTLN("[Manual] Event: USB Connected. Changing to PowerOffState.");
            EVENT_WARN("Man:USB->PwOff");
            changingState = true;
            manager->changeState(new PowerOffState());
            break;
            
        case EVENT_POWER_SWITCH_OFF:
            LOG_PRINTLN("[Manual] Event: Power Switch OFF. Changing to PowerOffState.");
            EVENT_WARN("Man:PWR->PwOff");
            changingState = true;
            manager->changeState(new PowerOffState());
            break;

        default:
            break;
    }
}

void ManualPlayState::update(StateManager* manager) {
    if (isWaitingReward) {
        unsigned long now = millis();
        unsigned long elapsed = now - rewardStartTime;
        
        if (elapsed >= rewardDelayMs) {
            LOG_PRINTF("[Manual][t=%lu] ¡Tiempo cumplido! elapsed=%lu ms >= target=%d ms\n", 
                       now, elapsed, rewardDelayMs);
            LOG_PRINTF("[Manual][t=%lu] Auto-disparando EVENT_TRAINING_SUCCESS...\n", now);
            isWaitingReward = false; // Apagamos el temporizador
            
            // Creamos un evento de éxito y lo inyectamos en nuestra propia cola
            Event ev;
            ev.type = EVENT_TRAINING_SUCCESS;
            xQueueSend(manager->getEventQueue(), &ev, 0); 
        }
    }
}

void ManualPlayState::saveRun(StateManager* manager, const char* result) {
    if (currentSession == nullptr) return;

    unsigned long saveStartTime = millis();
    LOG_PRINTF("\n[Manual][t=%lu] ========== GUARDANDO DISPARO: %s ==========\n", saveStartTime, result);

    currentSession->setResult(result);
    currentSession->setDeviceCode(dataManager->getDeviceID());
    
    // Obtener datos ambientales del sensor
    unsigned long envReadStart = millis();
    EnvData envData = manager->getHardwareManager()->getEnvironmentData();
    unsigned long envReadTime = millis() - envReadStart;
    
    if (envData.valid) {
        // Crear JSON string con los datos ambientales
        String conditionsJson = "{\"temp\":" + String(envData.temperature, 1) + 
                                ",\"humidity\":" + String(envData.humidity, 1) + 
                                ",\"pressure\":" + String(envData.pressure, 1) + "}";
        currentSession->setConditions(conditionsJson);
        LOG_PRINTF("[Manual] Condiciones ambientales: %s (lectura: %lu ms)\n", 
                   conditionsJson.c_str(), envReadTime);
    } else {
        LOG_PRINTF("[Manual] WARNING: Sensor ambiental no válido (lectura: %lu ms)\n", envReadTime);
    }
    
    // 2. Serializar y Guardar en LittleFS
    unsigned long serializeStart = millis();
    String json;
    if (currentSession->serialize(json)) {
        unsigned long serializeTime = millis() - serializeStart;
        LOG_PRINTF("[Manual] Serialización completada en %lu ms (json size: %d bytes)\n", 
                   serializeTime, json.length());
        
        unsigned long saveFileStart = millis();
        if (dataManager->saveSessionToChunk(json, currentSession->getStartedAt())) {
            unsigned long saveFileTime = millis() - saveFileStart;
            LOG_PRINTF(">> Tiro Manual guardado: %s (archivo: %lu ms)\n", result, saveFileTime);
        } else {
            LOG_PRINTLN("[Manual] ERROR: Fallo al guardar archivo");
        }
    } else {
        LOG_PRINTLN("[Manual] ERROR: Fallo al serializar sesión");
    }

    // 3. LIMPIAR PARA EL SIGUIENTE DISPARO
    currentSession->setResult("");
    
    // Adelantamos el reloj base con un mínimo entre disparos
    unsigned long now = millis();
    unsigned long rawElapsedMs = now - roundStartMillis;
    int elapsedSeconds = rawElapsedMs / 1000;
    int advanceSeconds = (elapsedSeconds < MIN_MANUAL_SHOT_INTERVAL_S)
        ? MIN_MANUAL_SHOT_INTERVAL_S
        : elapsedSeconds;
    
    // Log detallado de tiempos
    LOG_PRINTF("\n[Manual][Time Analysis] ========== ANÁLISIS DE TIEMPOS ==========\n");
    LOG_PRINTF("[Manual][Time] now             = %lu ms\n", now);
    LOG_PRINTF("[Manual][Time] roundStartMillis = %lu ms\n", roundStartMillis);
    LOG_PRINTF("[Manual][Time] rawElapsedMs     = %lu ms (%.2f s)\n", rawElapsedMs, rawElapsedMs/1000.0);
    LOG_PRINTF("[Manual][Time] elapsedSeconds   = %d s\n", elapsedSeconds);
    LOG_PRINTF("[Manual][Time] MIN_INTERVAL     = %d s\n", MIN_MANUAL_SHOT_INTERVAL_S);
    LOG_PRINTF("[Manual][Time] advanceSeconds   = %d s (aplicado)\n", advanceSeconds);
    
    unsigned long totalSaveTime = millis() - saveStartTime;
    LOG_PRINTF("[Manual][Time] Tiempo total saveRun() = %lu ms\n", totalSaveTime);
    LOG_PRINTF("[Manual][Time] ============================================\n\n");
    
    currentSession->addSecondsToTimeStamp(advanceSeconds);
    
    roundStartMillis = millis();
    LOG_PRINTF("[Manual] roundStartMillis actualizado = %lu. Listo para el próximo tiro.\n\n", roundStartMillis);
}
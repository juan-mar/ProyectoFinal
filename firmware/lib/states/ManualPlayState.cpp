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

// Constructor actualizado con RemoteControl
ManualPlayState::ManualPlayState(DataManager* dm, TrainingSession* session)
    : dataManager(dm), currentSession(session),changingState(false)
{
}

void ManualPlayState::enter(StateManager* manager) {
    LOG_PRINTLN("Entering ManualPlayState (Multi-Shot Mode)...");
    changingState = false; // Escudo Anti-Zombis

    // 1. Inicializar HW del control remoto
    manager->getHardwareManager()->sendCommand(CMD_REMOTE_POWER_ON);
    
    // 2. Esperar estabilización
    vTaskDelay(50 / portTICK_PERIOD_MS); 
}



void ManualPlayState::execute(StateManager* manager) {
    Event event;
    
    // Bloqueo indefinido: Esperamos a que el Hardware (RemoteControl)
    // o el Usuario (Interruptor) nos mande un evento a la cola.
    if (xQueueReceive(manager->getEventQueue(), &event, portMAX_DELAY) == pdTRUE) {
        handleEvent(manager, event);
    }
    
}

void ManualPlayState::exit(StateManager* manager) {
    LOG_PRINTLN("Exiting ManualPlayState...");

    // 1. Detener Software y HW del control remoto
    // Send CMD

    // 2. Apagar Hardware (Ahorro de energía)
    //Send CMD 
    //manager->getUserInterface()->setRemoteRxPower(false);

    // 3. Limpiar Sesión
    if (currentSession != nullptr) {
        delete currentSession;
        currentSession = nullptr;
    }
}
void ManualPlayState::handleEvent(StateManager* manager, Event& event) {
    if (changingState) return;

    switch (event.type) {
        
        // --- BOTÓN 1: BIEN (Éxito) ---
        case EVENT_TRAINING_SUCCESS:
            LOG_PRINTLN("[Manual] Botón BIEN presionado. Guardando éxito...");
            
            // Opcional: Aquí podrías mandar la orden al Hardware de disparar el premio
            // manager->getHardwareManager()->sendCommand(CMD_SOLENOID_FIRE);
            
            saveRun("success");
            // No cambiamos de estado. Se queda esperando el próximo botón.
            break;

        // --- BOTÓN 2: MAL (Fallo) ---
        case EVENT_TRAINING_FAILED:
            LOG_PRINTLN("[Manual] Botón MAL presionado. Guardando fallo...");
            saveRun("fail");
            // No cambiamos de estado. Se queda esperando el próximo botón.
            break;

        // --- BOTÓN 3: FIN (Salir) ---
        case EVENT_PLAY_FINISHED:
        case EVENT_MODE_ONLINE_ACTIVATED:
            LOG_PRINTLN("[Manual] Fin del entrenamiento. Saliendo a Config...");
            changingState = true;
            manager->changeState(new ConfigState(dataManager, manager->getWebServerManager()));
            break;

        default:
            break;
    }
}

void ManualPlayState::update(StateManager* manager) {

}

void ManualPlayState::saveRun(const char* result) {
    if (currentSession == nullptr) return;
    
    // 2. Completar datos
    currentSession->setResult(result);
    // Nota: setStartedAt ya se configuró en ConfigState con la hora del celular

    // 3. Serializar y Guardar
    String json;
    if (currentSession->serialize(json)) {
        if (dataManager->saveSessionFile(json)) {
            LOG_PRINTLN(">> Run saved to LittleFS.");
        } else {
            LOG_PRINTLN(">> ERROR saving run.");
        }
    }

    // 4. LIMPIAR PARA EL SIGUIENTE DISPARO (Multi-run)
    // Mantenemos al perro configurado, pero borramos resultado
    currentSession->setResult("");
    
    LOG_PRINTLN("Ready for next shot...");
}
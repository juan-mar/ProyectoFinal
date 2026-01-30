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
    : dataManager(dm), currentSession(session)
{
}

void ManualPlayState::enter(StateManager* manager) {
    LOG_PRINTLN("Entering ManualPlayState...");
    
    // 1. Inicializar HW del control remoto
    //Send CMD
    //manager->getHardwareManager()->setRemoteRxPower(true);
    
    // 2. Esperar estabilización (IMPORTANTE)
    // Los módulos de radio tardan unos ms en arrancar al recibir energía.
    vTaskDelay(50 / portTICK_PERIOD_MS); 

    //3. Indicacion led del estado actual
    //Send CMD
    //manager->getUserInterface()->setLedPattern(LED_SUCCESS);
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
    
    switch (event.type) {
        
        // --- CASO 1: ÉXITO (El hardware disparó el premio) ---
        case EVENT_TRAINING_SUCCESS:
            LOG_PRINTLN("[Manual] Reward Dispensed Successfully!");
            // Feedback Visual opcional (ej. parpadeo rápido)
            // manager->getUserInterface()->flashGreen(); 
            saveRun("success");
            break;

        // --- CASO 2: FALLO (El hardware intentó pero falló) ---
        case EVENT_TRAINING_FAILED:
            LOG_PRINTLN("[Manual] Reward Trigger Failed (Jam/Error).");
            // Feedback Visual de error
            //manager->getUserInterface()->setLedPattern(LED_ERROR_DB); 
            saveRun("fail");
            break;

        // --- CASO 3: FIN DEL JUEGO (Botón "Fin" presionado en control remoto) ---
        case EVENT_PLAY_FINISHED:
            LOG_PRINTLN("[Manual] Game Finished by user.");
            
            // Volvemos a Config (esto llamará a nuestro exit() y limpiará memoria)
            // IMPORTANTE: Necesitamos pasarle el WebServerManager al ConfigState.
            // Asumimos que StateManager tiene el getter configurado.
            manager->changeState(new ConfigState(dataManager, manager->getWebServerManager()));
            break;

        // --- CASO 4: INTERRUPTOR FÍSICO (Modo Online activado) ---
        case EVENT_MODE_ONLINE_ACTIVATED:
            LOG_PRINTLN("[Manual] Switch moved to Online. Aborting game.");
            // Volvemos a Config primero para una salida limpia
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
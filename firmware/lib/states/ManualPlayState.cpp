/****************************************************************
 * @file ManualPlayState.cpp
 * @brief Implements ManualPlayState logic.
 ****************************************************************/

#include "ManualPlayState.h"
#include "StateManager.h"
#include "DataManager.h"
#include "TrainingSession.h"
#include "RemoteControl.h" 
#include "UserInterface.h"
#include "ConfigState.h"
#include "Events.h"
#include "config.h"

// Constructor actualizado con RemoteControl
ManualPlayState::ManualPlayState(DataManager* dm, TrainingSession* session)
    : dataManager(dm), remoteControl(nullptr), currentSession(session)
{
}

void ManualPlayState::enter(StateManager* manager) {
    LOG_PRINTLN("Entering ManualPlayState...");
    
    // 1. Encender el Hardware (Power Gating)
    manager->getUserInterface()->setRemoteRxPower(true);
    
    // 2. Esperar estabilización (IMPORTANTE)
    // Los módulos de radio tardan unos ms en arrancar al recibir energía.
    vTaskDelay(50 / portTICK_PERIOD_MS); 

    // 3. Crear el Driver (Software)
    this->remoteControl = new RemoteControl();
    
    // 4. Iniciar la lógica del Driver
    if (this->remoteControl != nullptr) {
        this->remoteControl->begin(manager->getEventQueue());
        LOG_PRINTLN("RemoteControl initialized.");
    }

    manager->getUserInterface()->setLedPattern(LED_SUCCESS);
    lastActionTime = millis();
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

    // 1. Detener y Borrar Driver (Software)
    if (this->remoteControl != nullptr) {
        this->remoteControl->stop();
        delete this->remoteControl; // Libera la memoria
        this->remoteControl = nullptr;
        LOG_PRINTLN("RemoteControl object deleted.");
    }

    // 2. Apagar Hardware (Ahorro de energía)
    manager->getUserInterface()->setRemoteRxPower(false);

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
            manager->getUserInterface()->setLedPattern(LED_ERROR_DB); 
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
    // Si la librería de radio necesita polling constante (no usa interrupciones),
    // llamamos a su update aquí. Si usa IRQ, esto puede quedar vacío.
    if (remoteControl != nullptr) {
        remoteControl->update();
    }
}

// --- Helper Privado ---
void ManualPlayState::saveRun(const char* result) {
    if (currentSession == nullptr) return;

    // 1. Calcular duración de ESTE disparo
    unsigned long now = millis();
    int durationSeconds = (now - lastActionTime) / 1000;
    
    // Evitar duraciones negativas o cero si es muy rápido
    if (durationSeconds < 1) durationSeconds = 1;
    
    lastActionTime = now; // Resetear timer para el siguiente disparo

    // 2. Completar datos
    currentSession->setResult(result);
    currentSession->setDuration(durationSeconds);
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
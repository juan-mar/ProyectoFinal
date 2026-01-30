/****************************************************************
 * @file IdleState.cpp
 * @brief Implements the IdleState logic for light sleep.
 ****************************************************************/

/****************************************************************
 * Headers
 ****************************************************************/
#include "IdleState.h"
#include "StateManager.h"
#include "DataManager.h"
#include "Events.h"
#include "config.h"
#include "esp_sleep.h" // Required for light sleep functions
#include "HardwareManager.h"
#include "SupabaseClient.h"

// States we can transition to
#include "ConfigState.h"
#include "SyncState.h"

/****************************************************************
 * Defines and Constants
 ****************************************************************/

#define WAKE_UP_PIN     PIN_MODE_SWITCH

/**
 * @brief Wakeup level for the switch.
 * Asumimos que IDLE ocurre en ONLINE (HIGH), y despertamos
 * si el usuario lo mueve a OFFLINE (LOW).
 * (0 = LOW, 1 = HIGH)
 */
#define MODE_SWITCH_WAKEUP_LEVEL 1 // Wake on high signal (Offline)

/****************************************************************
 * Class Method Implementations
 ****************************************************************/

IdleState::IdleState(){
}

void IdleState::enter(StateManager* manager) {
    LOG_PRINTLN("FSM: Entering IdleState...");

    // Apagar LEDs visualmente antes de dormir (Opcional, o mandar comando CMD_LEDS_OFF)
    // manager->getHardware()->sendCommand(CMD_SET_LED_PATTERN, LED_OFF);

    // 1. Delegar el sueño al HardwareManager
    // Esta línea detiene la FSM hasta que alguien mueva el interruptor
    Event wakeupEvent = manager->getHardwareManager()->enterLightSleep();

    // 2. Al volver, ya tenemos el evento cocinado. Lo enviamos a la cola.
    // (Opcional: Podrías procesarlo directo, pero enviarlo a la cola mantiene el flujo 'execute')
    LOG_PRINTF("FSM: Woke up with event type: %d\n", wakeupEvent.type);
    
    xQueueSend(manager->getEventQueue(), &wakeupEvent, 0);
    
    // La FSM saldrá de IdleState en la próxima vuelta del loop execute()
    // cuando lea este evento de la cola.
}

void IdleState::execute(StateManager* manager) {
    Event event;
    QueueHandle_t queue = manager->getEventQueue();

    // Este estado es puramente reactivo. Duerme (la tarea)
    // indefinidamente hasta que el evento de 'enter()' (después
    // de despertar) o cualquier otro evento llegue.
    if (xQueueReceive(queue, &event, portMAX_DELAY) == pdTRUE) {
        handleEvent(manager, event);
    }
}

void IdleState::exit(StateManager* manager) {
    LOG_PRINTLN("Exiting IdleState...");
    
    manager->getHardwareManager()->prepareForWakeUp();
}

/****************************************************************
 * Protected Methods
 ****************************************************************/

void IdleState::handleEvent(StateManager* manager, Event& event) {
    switch (event.type) {
        case EVENT_MODE_OFFLINE_ACTIVATED:
            LOG_PRINTLN("[IdleState] Event: Mode OFFLINE. Changing to ConfigState.");
            manager->changeState(new ConfigState(manager->getDataManager(), manager->getWebServerManager()));
            break;
        case EVENT_MODE_ONLINE_ACTIVATED:
            LOG_PRINTLN("[IdleState] Event: Mode ONLINE. Changing to SyncState.");
            manager->changeState(new SyncState(manager->getDataManager(), manager->getSupabaseClient()));
            break;
        default:
            break;
    }
}

void IdleState::update(StateManager* manager) {
    // Este estado no tiene lógica periódica.
    // El 'execute()' nunca lo llama.
}
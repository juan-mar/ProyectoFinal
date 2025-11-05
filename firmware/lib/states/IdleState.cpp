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

// States we can transition to
#include "ConfigState.h"

/****************************************************************
 * Defines and Constants
 ****************************************************************/
/**
 * @brief GPIO pin for the mode switch (ONLINE/OFFLINE).
 * !! EDITAR ESTE PIN PARA QUE COINCIDA CON TU HARDWARE !!
 */
#define MODE_SWITCH_PIN 25

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

IdleState::IdleState(DataManager* dataManager) 
    : dataManager(dataManager)
{
    // Constructor stores the pointer
}

void IdleState::enter(StateManager* manager) {
    LOG_PRINTLN("Entering IdleState...");
    LOG_PRINTLN("Configuring wake-up sources...");

    // 1. Configurar el pin del interruptor como fuente de despertar
    gpio_wakeup_enable((gpio_num_t)MODE_SWITCH_PIN, 
        (MODE_SWITCH_WAKEUP_LEVEL == 0) ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL);
    
    esp_sleep_enable_gpio_wakeup();
    
    
    
    PIN_HIGH(2); // Turn off debug LED to indicate sleep

    // 2. Entrar en modo de sueño ligero
    LOG_PRINTLN("Going to light sleep. Zzz...");
    LOG_FLUSH();

    esp_light_sleep_start();

    // --- ¡EL CÓDIGO SE REANUDA AQUÍ DESPUÉS DE DESPERTAR! ---
    LOG_PRINTLN("Woke up from light sleep!");

    // 3. Averiguar por qué despertamos
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    Event ev;
    
    if (cause == ESP_SLEEP_WAKEUP_GPIO) {
        LOG_PRINTLN("Wakeup caused by GPIO (Mode Switch).");
        // Asumimos que el evento es ir a OFFLINE
        ev.type = EVENT_MODE_OFFLINE_ACTIVATED;
    } else if (cause == ESP_SLEEP_WAKEUP_UART) {
        LOG_PRINTLN("Wakeup cause UART.");
        // En el arranque inicial, vamos a ConfigState
        ev.type = EVENT_MODE_OFFLINE_ACTIVATED;
    } else  {
        LOG_PRINT("Woke up for unknown reason.");
        LOG_PRINTLN(cause);
        
        // Por seguridad, volvemos a ConfigState
        ev.type = EVENT_MODE_OFFLINE_ACTIVATED;
    }
    
    // 4. Enviar el evento a nuestra propia cola para ser procesado por execute()
    xQueueSend(manager->getEventQueue(), &ev, 0);
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
    
    // Limpiar las fuentes de despertar de hardware
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
}

/****************************************************************
 * Protected Methods
 ****************************************************************/

void IdleState::handleEvent(StateManager* manager, Event& event) {
    switch (event.type) {
        case EVENT_MODE_OFFLINE_ACTIVATED:
            LOG_PRINTLN("[IdleState] Event: Mode OFFLINE. Changing to ConfigState.");
            manager->changeState(new ConfigState(dataManager));
            break;

        default:
            // Ignorar otros eventos (ej. sync_completed, etc.)
            break;
    }
}

void IdleState::update(StateManager* manager) {
    // Este estado no tiene lógica periódica.
    // El 'execute()' nunca lo llama.
}
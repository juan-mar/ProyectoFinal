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
#include "UserInterface.h"
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
    LOG_PRINTLN("Entering IdleState...");
    LOG_PRINTLN("Configuring wake-up sources...");
    manager->getUserInterface()->setLedPattern(LED_OFF);
    
    // 1. Configurar Interruptor para wake-up
    manager->getUserInterface()->disableSwitchInterrupt();

    int currentSwitchState = digitalRead(WAKE_UP_PIN);
    gpio_int_type_t wakeupLevel;
    if (currentSwitchState == HIGH) {
        wakeupLevel = GPIO_INTR_LOW_LEVEL;
        LOG_PRINTLN("Switch is HIGH. Sleeping until it goes LOW.");
    } else {
        wakeupLevel = GPIO_INTR_HIGH_LEVEL;
        LOG_PRINTLN("Switch is LOW. Sleeping until it goes HIGH.");
    }
    gpio_wakeup_enable((gpio_num_t)WAKE_UP_PIN, wakeupLevel);
    esp_sleep_enable_gpio_wakeup();
    
    PIN_HIGH(2); // Turn off debug LED to indicate sleep

    // 2. Entrar en modo de sueño ligero
    LOG_PRINTLN("Going to light sleep. Zzz...");
    LOG_FLUSH();
    esp_light_sleep_start();

    LOG_PRINTLN("Woke up from light sleep!");
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    bool isNowOnline = digitalRead(WAKE_UP_PIN);
    Event ev;
    
    if (cause == ESP_SLEEP_WAKEUP_GPIO) {
        LOG_PRINTLN("Wakeup caused by GPIO (Mode Switch).");
        ev.type = isNowOnline ? EVENT_MODE_ONLINE_ACTIVATED : EVENT_MODE_OFFLINE_ACTIVATED;
    } else if (cause == ESP_SLEEP_WAKEUP_UART) {
        LOG_PRINTLN("Wakeup cause UART.");
        ev.type = isNowOnline ? EVENT_MODE_ONLINE_ACTIVATED : EVENT_MODE_OFFLINE_ACTIVATED;
    } else  {
        LOG_PRINT("Woke up for unknown reason: ");
        LOG_PRINTLN(cause);
        ev.type = isNowOnline ? EVENT_MODE_ONLINE_ACTIVATED : EVENT_MODE_OFFLINE_ACTIVATED;
    }
    
    // 3. Enviar el evento a nuestra propia cola para ser procesado por execute()
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
    manager->getUserInterface()->enableSwitchInterrupt();
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
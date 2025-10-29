/****************************************************************
 * @file IdleState.cpp
 * @brief Implements the IdleState logic for light sleep.
 ****************************************************************/
#include "IdleState.h"
#include "StateManager.h"
#include "Events.h"
#include <Arduino.h>
#include "esp_sleep.h" // Required for light sleep functions

// Include states we can transition to
#include "ConfigState.h"
#include "SyncState.h"

// --- Define el pin de tu interruptor de MODO ---
// Asumimos que OFFLINE es LOW y ONLINE es HIGH
#define MODE_SWITCH_PIN 25
#define MODE_SWITCH_WAKEUP_LEVEL 1 // 1 = HIGH, 0 = LOW

void IdleState::enter(StateManager* manager) {
    Serial.println("Entering IdleState...");
    Serial.println("Configuring wake-up sources...");

    // 1. Configurar la fuente de despertar
    // Queremos despertar si el pin del interruptor cambia.
    // Aquí asumimos que el estado IDLE ocurre en modo ONLINE (interruptor en HIGH),
    // por lo que queremos despertar si se mueve a LOW (OFFLINE).
    
    // Si tu lógica es al revés (Idle en modo OFFLINE), cambia el nivel.
    // Para este ejemplo, asumimos que estamos en ONLINE y despertamos si va a OFFLINE (LOW = 0)
    // gpio_wakeup_enable((gpio_num_t)MODE_SWITCH_PIN, GPIO_INTR_LOW_LEVEL);
    
    // --- VAMOS A SIMPLIFICAR ---
    // Despertar si el interruptor se mueve a OFFLINE (asumiendo que OFFLINE es LOW)
    gpio_wakeup_enable((gpio_num_t)MODE_SWITCH_PIN, GPIO_INTR_LOW_LEVEL); 
    esp_sleep_enable_gpio_wakeup();

    // 2. Entrar en modo de sueño ligero
    Serial.println("Going to light sleep. Zzz...");
    esp_light_sleep_start();

    // --- ¡EL CÓDIGO SE REANUDA AQUÍ DESPUÉS DE DESPERTAR! ---
    Serial.println("Woke up from light sleep!");

    // 3. Averiguar por qué despertamos y enviar el evento
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause == ESP_SLEEP_WAKEUP_GPIO) {
        Serial.println("Wakeup caused by GPIO.");
        // Asumimos que fue el interruptor de modo moviéndose a OFFLINE
        Event ev;
        ev.type = EVENT_MODE_OFFLINE_ACTIVATED;
        xQueueSend(manager->getEventQueue(), &ev, 0);
    } else {
        Serial.println("Woke up for unknown reason.");
        // Podríamos volver a dormir, pero por seguridad, mejor
        // volvemos a ConfigState.
        Event ev;
        ev.type = EVENT_MODE_OFFLINE_ACTIVATED;
        xQueueSend(manager->getEventQueue(), &ev, 0);
    }
}

void IdleState::execute(StateManager* manager) {
    // 4. El execute() solo espera el evento que nos auto-enviamos
    //    desde enter() después de despertar.
    Event event;
    QueueHandle_t queue = manager->getEventQueue();

    // Usamos bloqueo indefinido. Esta tarea dormirá (Task Sleep)
    // hasta que el evento de 'enter()' llegue.
    if (xQueueReceive(queue, &event, portMAX_DELAY) == pdTRUE) {
        
        switch (event.type){
            case EVENT_MODE_OFFLINE_ACTIVATED:
                Serial.println("[IdleState] Event: Mode OFFLINE. Changing to ConfigState.");
                manager->changeState(new ConfigState(/* webServer, etc */));
                break;
            case EVENT_MODE_ONLINE_ACTIVATED:
                Serial.println("[IdleState] Event: Mode ONLINE. Changing to SyncState.");
                manager->changeState(new SyncState(/* supabaseManager, etc */));
                break;
            default:
                break;
        }
    }
}

void IdleState::exit(StateManager* manager) {
    Serial.println("Exiting IdleState...");
    // 5. Limpiar las fuentes de despertar
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
}
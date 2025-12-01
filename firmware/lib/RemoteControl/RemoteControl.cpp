/****************************************************************
 * @file RemoteControl.cpp
 * @brief Implements RemoteControl logic.
 ****************************************************************/

#include "RemoteControl.h"
#include "Events.h"
#include "config.h"

RemoteControl::RemoteControl() : fsmQueue(nullptr), isRunning(false) {
}

void RemoteControl::begin(QueueHandle_t queue) {
    LOG_PRINTLN("RemoteControl: Initializing RF Hardware...");
    this->fsmQueue = queue;
    this->isRunning = true;

    // TODO: Inicializar NRF24L01 aquí
    // radio.begin();
    // radio.openReadingPipe(...);
    // radio.startListening();
}

void RemoteControl::stop() {
    LOG_PRINTLN("RemoteControl: Stopping RF Hardware...");
    this->isRunning = false;
    
    // TODO: Apagar radio
    // radio.powerDown();
}

void RemoteControl::update() {
    if (!isRunning) return;

    // TODO: Lógica real de lectura del NRF24
    /*
    if (radio.available()) {
        char text[32] = "";
        radio.read(&text, sizeof(text));
        
        if (strcmp(text, "BTN_TRIGGER") == 0) {
            sendEvent(EVENT_TRAINING_SUCCESS); // O EVENT_TRIGGER_REWARD
        }
        else if (strcmp(text, "BTN_FINISH") == 0) {
            sendEvent(EVENT_PLAY_FINISHED);
        }
    }
    */
}

void RemoteControl::sendEvent(int type) {
    if (fsmQueue != nullptr) {
        Event ev;
        ev.type = (EventType)type;
        xQueueSend(fsmQueue, &ev, 0);
    }
}
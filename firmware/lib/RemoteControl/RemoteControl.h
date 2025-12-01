    /****************************************************************
 * @file RemoteControl.h
 * @brief Hardware driver for the RF Remote Control (e.g., NRF24).
 * Handles receiving signals and translating them to FSM events.
 ****************************************************************/

#ifndef REMOTE_CONTROL_H
#define REMOTE_CONTROL_H

#include <Arduino.h>
#include <freertos/queue.h>

class RemoteControl {
public:
    RemoteControl();

    /**
     * @brief Inicializa el hardware de radio (NRF24).
     * @param fsmQueue Cola para enviar eventos (TRIGGER, FINISH).
     */
    void begin(QueueHandle_t fsmQueue);

    /**
     * @brief Apaga la radio o la pone en bajo consumo.
     */
    void stop();

    /**
     * @brief Polling loop. Llamar si la librería de radio requiere
     * revisar datos constantemente (no usa interrupciones).
     */
    void update();

private:
    QueueHandle_t fsmQueue;
    bool isRunning;
    
    // Aquí irían tus objetos de hardware, ej:
    // RF24 radio; 
    
    // Helper para enviar eventos a la FSM
    void sendEvent(int type);
};

#endif // REMOTE_CONTROL_H
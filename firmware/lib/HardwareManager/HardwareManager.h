#ifndef HARDWARE_MANAGER_H
#define HARDWARE_MANAGER_H

#include <Arduino.h>
#include <freertos/queue.h>
#include "Events.h"       // Para poder enviar eventos a la FSM
#include "RemoteControl.h" // Tu driver de NRF24
//#include "BleScanner.h"    // Tu nuevo driver de BLE

// --- COMANDOS: Órdenes que recibe el HardwareManager ---
enum HwCmdType {
    CMD_NOOP,
    
    // Actuadores
    CMD_FIRE_SOLENOID,      // Disparar pelota
    CMD_SET_LED_PATTERN,    // Cambiar luces
    
    // Gestión de Energía / Drivers
    CMD_ENABLE_REMOTE,      // Prender radio NRF24
    CMD_DISABLE_REMOTE,     // Apagar radio NRF24
    CMD_ENABLE_BLE,         // Iniciar escaneo BLE
    CMD_DISABLE_BLE         // Detener escaneo BLE
};

// Estructura del mensaje en la cola de comandos
struct HwMessage {
    HwCmdType command;
    int parameter; // Ej: LedPattern enum o duración extra
};

class HardwareManager {
public:
    HardwareManager();

    // Inicialización: Recibe la cola de la FSM para poder hablarle
    void init(QueueHandle_t fsmQueue);

    // Método Thread-Safe para que la FSM le mande órdenes
    bool sendCommand(HwCmdType cmd, int param = 0);

    // Método principal que se llama desde el loop de la Task
    void update();

    /**
     * @brief Pone el sistema en suspensión ligera.
     * Esta función BLOQUEA la ejecución hasta que el sistema despierta.
     * @return Event El evento que causó el despertar (ej: MODE_ONLINE o MODE_OFFLINE).
     */
    Event enterLightSleep();
    // Limpiar configuración de sleep y reactivar periféricos
    void prepareForWakeUp();

    //Constantes Publicas
    static const uint32_t LOOP_PERIOD_MS = 20;

private:
    // Colas
    QueueHandle_t _fsmQueue;       // Salida -> FSM
    QueueHandle_t _commandQueue;   // Entrada <- FSM

    // Drivers Internos
    RemoteControl _remoteControl;
    //BleScanner    _bleScanner;

    // Estado Interno
    bool _remoteEnabled;
    bool _bleEnabled;
    
    // Solenoide
    bool _solenoidActive;
    unsigned long _solenoidOffTime;

    // Sensores
    unsigned long _lastSensorCheck;

    static const uint8_t PIN_MODE_SWITCH = 25;

    // Métodos Privados de ayuda
    void processCommand(HwMessage msg);
    void updateActuators();
    void checkDrivers();
    void readSensors(); // Batería, Temperatura
};

#endif // HARDWARE_MANAGER_H
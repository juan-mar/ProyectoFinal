#ifndef HARDWARE_MANAGER_H
#define HARDWARE_MANAGER_H

/****************************************************************
 * Headers
 ****************************************************************/
#include <Arduino.h>
#include <freertos/queue.h>
#include "Events.h"                 // Para poder enviar eventos a la FSM
#include "RemoteControl.h"          // Tu driver de NRF24
#include "HardwareConfig.h"         // Configuración centralizada de pines
#include "rx.h"                     // Tu nuevo driver de BLE
#include "BatteryMonitor.h"         // Battery monitoring
#include "EnvironmentSensor.h"      // BME280 environment sensor

/****************************************************************
 * @brief Hardware Command Types
 * Órdenes que otros módulos pueden enviar a HardwareManager
 * para controlar actuadores y periféricos desde el thread de HW.
 ****************************************************************/
enum HwCmdType {
    CMD_NOOP = 0,
    
    // --- TAG / RFID Control ---
    CMD_TAG_POWER_ON,               // Encender TAG (lector RFID)
    CMD_TAG_POWER_OFF,              // Apagar TAG (ahorrar energía)
    //CMD_TAG_CALIBRATION_MODE,       // Entrar modo calibración
    //CMD_TAG_DETECTION_MODE,         // Entrar modo detección normal
    
    // --- Remote Control (NRF24) ---
    CMD_REMOTE_POWER_ON,            // Encender receptor NRF24
    CMD_REMOTE_POWER_OFF,           // Apagar receptor NRF24
    
    // --- LEDs Control ---
    CMD_LED_SEQUENCE_START,         // Iniciar secuencia de LEDs (ej: calibración)
    CMD_LED_SEQUENCE_STOP,          // Detener secuencia
    CMD_LED_SET_PATTERN,            // Cambiar patrón LED fijo (parameter: patrón)
    CMD_LED_OFF,                    // Apagar todos los LEDs
    
    // --- Solenoid / Reward Dispenser ---
    CMD_SOLENOID_FIRE,              // Disparar solenoide (entregar reward)
    
    // --- Launcher Control ---
    CMD_LAUNCHER_ON,                // Encender lanzador (pin digital)
    CMD_LAUNCHER_OFF,               // Apagar lanzador
    

};

#define CMD_TAG_PARAM_CALIBRATION   1
#define CMD_TAG_PARAM_DETECTION     0

/****************************************************************
 * @brief LED Pattern Types
 * Patrones predefinidos para los LEDs
 ****************************************************************/
enum LedPattern {
    LED_OFF = 0,
    LED_IDLE,                       // Parpadeo lento (standby)
    LED_CALIBRATION,                // Secuencia parpadeante rápida
    LED_ACTIVE,                     // LEDs fijos encendidos
    LED_ERROR,                      // Parpadeo rojo urgente
    LED_SUCCESS                     // Parpadeo verde rápido
};

/****************************************************************
 * @brief Hardware Command Message
 * Estructura enviada por la cola de comandos del HardwareManager
 ****************************************************************/
struct HwMessage {
    HwCmdType command;
    int parameter;                  // Ej: LedPattern, duración, etc.
    unsigned long timestamp;        // Timestamp del comando
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
    static const uint32_t BATTERY_READ_PERIOD_MS = 5000;      // Leer batería cada ~5 segundos
    static const uint32_t ENVIRONMENT_READ_PERIOD_MS = 60000;  // Leer sensor ambiental cada ~60 segundos

    /**
     * @brief Obtiene el porcentaje actual de batería.
     * @return Porcentaje de batería (0-100), o -1 si no está inicializado.
     */
    int getBatteryPercentage();

    /**
     * @brief Obtiene las lecturas actuales del sensor ambiental.
     * @return Estructura EnvData con temperatura, humedad, presión y flag de validez.
     */
    EnvData getEnvironmentData();

private:
    // Colas
    QueueHandle_t _fsmQueue;       // Salida -> FSM
    QueueHandle_t _commandQueue;   // Entrada <- FSM

    // Drivers Internos
    RemoteControl _remoteControl;
    Receptor _bleScanner;
    BatteryMonitor _batteryMonitor{PIN_BATTERY, BATTERY_MULTIPLIER};
    EnvironmentSensor _environmentSensor;

    // --- Estado de Periféricos ---
    struct PeripheralState {
        bool tagEnabled;            // TAG/RFID reader on/off
        bool remoteEnabled;         // Remote NRF24 on/off
    } _peripheralState;

    // --- Estado de Mode Switches ---
    struct ModeSwitchState {
        bool prevStateA;            // Estado anterior PIN_MODE_SWITCH_A
        bool prevStateM;            // Estado anterior PIN_MODE_SWITCH_M
    } _modeSwitchState;

    // --- Estado de Control Remoto (Detección Doble BTN1) ---
    struct RemoteButtonState {
        bool btn1Pending;           // BTN1 esperando confirmación
        unsigned long btn1FirstPressTime; // Timestamp del primer BTN1
        static const unsigned long BTN1_DOUBLE_PRESS_WINDOW_MS = 1000; // 1 segundo
    } _remoteButtonState;

    // --- Estado de Actuadores ---
    struct ActuatorState {
        bool solenoidActive;        // Solenoide disparándose
        unsigned long solenoidOffTime;  // Cuándo apagar el solenoide
        bool solenoidCooldownActive; // Cooldown activo para bloquear re-disparo
        unsigned long solenoidCooldownStartMs; // Inicio del cooldown
        
        bool launcherActive;        // Lanzador encendido
        unsigned long launcherEN1OnTime; // Cuándo se encendió EN_1
        bool launcherEN2Pending;    // Espera para encender EN_2
        
        LedPattern currentLedPattern;   // Patrón LED actual
        bool ledSequenceRunning;    // Secuencia de LEDs en curso
        unsigned long ledBlinkRate; // Velocidad de parpadeo
        unsigned long ledLastToggle;    // Último cambio de LED
        bool ledCurrentState;       // Estado actual (on/off)
    } _actuatorState;

    // --- Timing ---
    unsigned long _lastBatteryReading;      // Último tiempo de lectura de batería
    unsigned long _lastEnvironmentReading;  // Último tiempo de lectura de sensor ambiental

    // Métodos Privados de ayuda
    void processCommand(HwMessage msg);
    
    // --- Actuators
    void updateActuators();
    void updateLeds();
    void updateSolenoid();
    void updateLauncher();
    
    // --- Drivers
    void checkDrivers();
    void update_tag();
    void update_remote();
    
    // --- Mode Switches
    void checkModeSwitches();
    
    // --- Sensors
    void readSensors();            // Batería, Temperatura

    
    // Métodos auxiliares específicos
    void enableTag(bool mode = false);  // mode: false=detección, true=calibración
    void disableTag();
    void fireSolenoid(unsigned long durationMs = SOLENOID_PULSE_DURATION_MS);
    void setLedPattern(LedPattern pattern);
    void startLedSequence(LedPattern pattern, unsigned long blinkRate);
    void stopLedSequence();
};

#endif // HARDWARE_MANAGER_H
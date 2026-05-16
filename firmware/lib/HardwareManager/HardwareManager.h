#ifndef HARDWARE_MANAGER_H
#define HARDWARE_MANAGER_H

/****************************************************************
 * Headers
 ****************************************************************/
#include <Arduino.h>
#include <freertos/queue.h>
#include "Events.h"                 // Para poder enviar eventos a la FSM
#include "DataManager.h"            // NVS access for persisted TAG calibration
#include "RemoteControl.h"          // Tu driver de NRF24
#include "HardwareConfig.h"         // Configuración centralizada de pines
#include "rx.h"                     // Tu nuevo driver de BLE
#include "BatteryMonitor.h"         // Battery monitoring
#include "EnvironmentSensor.h"      // BME280 environment sensor
#include "MessageInterface.h"       // User messaging output abstraction

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
    
    // --- User Message Interface ---
    CMD_MSG_SET,                    // parameter: UserMessage
    CMD_MSG_OFF,                    // Shortcut a USER_MSG_OFF

    // --- Solenoid / Reward Dispenser ---
    CMD_SOLENOID_FIRE,              // Disparar solenoide (entregar reward)
    
    // --- Launcher Control ---
    CMD_LAUNCHER_ON,                // Encender lanzador (pin digital)
    CMD_LAUNCHER_OFF,               // Apagar lanzador
    

};

#define CMD_TAG_PARAM_CALIBRATION   1
#define CMD_TAG_PARAM_DETECTION     0


/****************************************************************
 * @brief Hardware Command Message
 * Estructura enviada por la cola de comandos del HardwareManager
 ****************************************************************/
struct HwMessage {
    HwCmdType command;
    int parameter;                  // Parámetro genérico del comando
    unsigned long timestamp;        // Timestamp del comando
};

class HardwareManager {
public:
    HardwareManager();

    // Inicialización: Recibe la cola de la FSM para poder hablarle
    void init(QueueHandle_t fsmQueue, DataManager* dataManager);

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
    
    /**
     * @brief Pone el sistema en suspensión profunda (deep sleep).
     * Esta función NO RETORNA - el dispositivo se reinicia al despertar.
      * Consumo mínimo: ~10µA. Despierta SOLO con power switch ON (nivel HIGH).
     */
    void enterDeepSleep();
    
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
     * @brief Obtiene el voltaje actual de batería.
     * @return Voltaje en V, o -1.0f si no está inicializado.
     */
    float getBatteryVoltage();

    /**
     * @brief Obtiene la categoría actual de batería.
     * @return HIGH, MEDIUM, LOW, CRITICAL o UNKNOWN.
     */
    const char* getBatteryLevelText();

    /**
     * @brief Obtiene las lecturas actuales del sensor ambiental.
     * @return Estructura EnvData con temperatura, humedad, presión y flag de validez.
     */
    EnvData getEnvironmentData();
    
    /**
     * @brief Notify HardwareManager when PowerOffState is active.
     * Enables GPIO monitoring for USB disconnect event during power-off sequence.
     * @param entering true = entering PowerOffState, false = exiting PowerOffState
     */
    void notifyPowerOffState(bool entering);

private:
    // Colas
    QueueHandle_t _fsmQueue;       // Salida -> FSM
    QueueHandle_t _commandQueue;   // Entrada <- FSM
    DataManager* _dataManager;

    // Drivers Internos
    RemoteControl _remoteControl;
    Receptor _bleScanner;
    MessageInterface _messageInterface;
    BatteryMonitor _batteryMonitor{PIN_BATTERY, BATTERY_MULTIPLIER};
    EnvironmentSensor _environmentSensor;

    // --- Estado de Periféricos ---
    struct PeripheralState {
        bool tagEnabled;            // TAG/RFID reader on/off
        bool remoteEnabled;         // Remote NRF24 on/off
    } _peripheralState;

    // --- Estado de Mode Switches y Power Status ---
    struct PowerStatusState {
        bool prevModeSwitch;        // Estado anterior PIN_MODE_SWITCH_ONLINE_OFFLINE
        bool prevPowerSwitch;       // Estado anterior PIN_POWER_SWITCH
        bool prevUsbConnected;      // Estado anterior PIN_USB_DETECT
    } _powerStatusState;

    // --- Estado de Control Remoto (Detección Doble BTN1) ---
    struct RemoteButtonState {
        bool btn1Pending;           // BTN1 esperando confirmación
        unsigned long btn1FirstPressTime; // Timestamp del primer BTN1
        static const unsigned long BTN1_DOUBLE_PRESS_WINDOW_MS = 1000; // 1 segundo
    } _remoteButtonState;
    
    // --- PowerOffState Monitoring ---
    bool _isInPowerOffState = false;  // True when PowerOffState is active, signals GPIO monitoring
    bool _powerSwitchOffEventLatched = false;  // One-shot latch while power switch stays LOW
    bool _powerOffReadyEventLatched = false;   // One-shot latch for EVENT_POWEROFF_READY_TO_SLEEP
    bool _batteryCriticalShutdownTriggered = false; // One-shot latch for critical battery shutdown

    // --- Estado de Actuadores ---
    struct ActuatorState {
        bool solenoidActive;        // Solenoide disparándose
        unsigned long solenoidOffTime;  // Cuándo apagar el solenoide
        bool solenoidCooldownActive; // Cooldown activo para bloquear re-disparo
        unsigned long solenoidCooldownStartMs; // Inicio del cooldown
        
        bool launcherActive;        // Lanzador encendido
        unsigned long launcherEN1OnTime; // Cuándo se encendió EN_1
        bool launcherEN2Pending;    // Espera para encender EN_2
    } _actuatorState;

    // --- Timing ---
    unsigned long _lastBatteryReading;      // Último tiempo de lectura de batería
    unsigned long _lastEnvironmentReading;  // Último tiempo de lectura de sensor ambiental

    // Métodos Privados de ayuda
    void processCommand(HwMessage msg);
    
    // --- Actuators
    void updateActuators();
    void updateMessageInterface();
    void updateSolenoid();
    void updateLauncher();
    
    // --- Drivers
    void checkDrivers();
    void update_tag();
    void update_remote();
    void loadPersistedTagCalibration();
    bool persistCurrentTagCalibration();
    
    // --- GPIO Status (mode switch, power switch, USB)
    void checkGPIOStatus();
    
    // --- Sensors
    void readSensors();            // Batería, Temperatura

    
    // Métodos auxiliares específicos
    void enableTag(bool mode = false);  // mode: false=detección, true=calibración
    void disableTag();
    void fireSolenoid(unsigned long durationMs = SOLENOID_PULSE_DURATION_MS);
    void blinkRedAndShutdown();
};

#endif // HARDWARE_MANAGER_H
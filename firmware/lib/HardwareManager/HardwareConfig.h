/****************************************************************
 * @file HardwareConfig.h
 * @brief Centralización de configuración de pines físicos y 
 * constantes del hardware. Editar aquí para cambiar asignaciones.
 ****************************************************************/

#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

/****************************************************************
 * PIN CONFIGURATION 
 ****************************************************************/

// --- Mode Switch (Hardware Interrupt) ---
/**
 * @brief Pin del switch de modo ONLINE/OFFLINE
 * Genera interrupción para despertar del light sleep
 */
#define PIN_MODE_SWITCH 25

// --- TAG / RFID Reader ---
/**
 * @brief Pin de enable/power para el lector RFID (TAG)
 * HIGH = encendido, LOW = apagado
 */
#define PIN_TAG_POWER 32

/**
 * @brief Pin de control de modo calibración/detección
 * HIGH = modo calibración, LOW = modo detección
 */
#define PIN_TAG_MODE 33

// --- Remote Control (NRF24) ---
/**
 * @brief Pin de enable/power para el módulo NRF24
 * HIGH = encendido, LOW = apagado
 */
#define PIN_REMOTE_POWER 27

// --- LEDs Control ---
/**
 * @brief Pin PWM/Digital para control de LEDs
 * (Si es PWM, permite control de intensidad)
 */
#define PIN_LED_CONTROL 12

/**
 * @brief Canal PWM para LEDs (si usas PWM)
 */
#define PWM_CHANNEL_LED 0
#define PWM_FREQUENCY_LED 1000     // Hz
#define PWM_BIT_WIDTH_LED 8         // 0-255

// --- Solenoid / Reward Dispenser ---
/**
 * @brief Pin de disparo del solenoide (carrera de gato)
 * HIGH = activado (dispara), LOW = desactivado
 */
#define PIN_SOLENOID 26

/**
 * @brief Duración por defecto del pulso del solenoide (ms)
 * Tiempo que mantiene HIGH antes de volver a LOW
 */
#define SOLENOID_PULSE_DURATION_MS 200

// --- Launcher Control ---
/**
 * @brief Pin de control del lanzador de pelotas
 * HIGH = encendido/disparando, LOW = apagado
 */
#define PIN_LAUNCHER 14

/**
 * @brief Duración del pulso de disparo del lanzador (ms)
 */
#define LAUNCHER_FIRE_DURATION_MS 150

// --- BLE Scanner (Placeholder) ---
/**
 * @brief Pin de control/status del módulo BLE (si aplica)
 * -1 si no se utiliza
 */
#define PIN_BLE_CONTROL -1

/****************************************************************
 * TIMING CONSTANTS
 ****************************************************************/

/**
 * @brief Período de actualización del HardwareManager task (ms)
 */
#define HARDWARE_LOOP_PERIOD_MS 20

/**
 * @brief Período de lectura de sensores (temperatura, batería)
 */
#define SENSOR_CHECK_INTERVAL_MS 1000

/**
 * @brief Timeout para comandos pendientes en la cola
 */
#define HW_COMMAND_QUEUE_SIZE 10
#define HW_COMMAND_QUEUE_TIMEOUT_MS 0  // 0 = no espera (descarta si llena)

/****************************************************************
 * LED SEQUENCE PATTERNS
 ****************************************************************/

/**
 * @brief Patrón predefinido para LEDs en modo IDLE
 * Parámetro en HwMessage.parameter
 */
#define LED_IDLE_BLINK_RATE_MS 1000

/**
 * @brief Patrón predefinido para LEDs en calibración
 * Parpadea más rápido que IDLE
 */
#define LED_CALIBRATION_BLINK_RATE_MS 300

/**
 * @brief Patrón predefinido para LEDs en error
 * Parpadeo urgente rojo
 */
#define LED_ERROR_BLINK_RATE_MS 150

/**
 * @brief Patrón predefinido para LEDs en éxito
 * Parpadeo verde confirmación
 */
#define LED_SUCCESS_BLINK_RATE_MS 200
#define LED_SUCCESS_DURATION_MS 2000  // Duración total de la animación

/****************************************************************
 * HARDWARE FEATURE FLAGS
 ****************************************************************/

/**
 * @brief Enable/Disable hardware features
 * Para compilación condicional en diferentes variantes
 */
#define ENABLE_TAG_READER 1
#define ENABLE_REMOTE_CONTROL 1
#define ENABLE_LED_CONTROL 0
#define ENABLE_SOLENOID 0
#define ENABLE_LAUNCHER 0
#define ENABLE_BLE_SCANNER 1

#endif // HARDWARE_CONFIG_H

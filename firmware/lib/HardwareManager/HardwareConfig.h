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
// --- Mode Switches ---
#define PIN_MODE_SWITCH_ONLINE_OFFLINE 14  // Online(HIGH)/Offline(LOW) switch
#define PIN_POWER_SWITCH 13                 // Power ON(HIGH)/OFF(LOW) switch

// --- USB Charging Detection ---
#define PIN_USB_DETECT 34                   // USB charging detection (HIGH=charging)


// --- Tag Reader (BLE) ---
//#define PIN_TAG_POWER 32

// --- Remote Control (NRF24) ---
#define PIN_NRF24_CE     17  // Cambiá este número por el pin físico que uses
#define PIN_NRF24_CSN    16  // Cambiá este número por el pin físico que uses
// Pines SPI por defecto (Conectar directo, la librería los maneja):
// MOSI -> GPIO 23
// MISO -> GPIO 19
// SCK  -> GPIO 18

// --- Solenoid / Reward Dispenser ---
#define PIN_SOLENOID 4      // Pin para recomppensa
#define PIN_LAUNCHER_1 33     // Pin para iniciar el lanzador - Banco de capacitores
#define PIN_LAUNCHER_2 32     // Pin para iniciar el lanzador - Banco de capacitores

// --- Environmental Sensor (BME280) ---
#define PIN_BME_SDA 21
#define PIN_BME_SCL 22
#define ENV_BME280_I2C_ADDRESS 0x76 // Dirección I2C del BME280 (0x76 o 0x77)

// --- LED Control ---
#define PIN_LED_R 27    //se vio verde
#define PIN_LED_G 25    //se vio azul
#define PIN_LED_B 26
#define LED_ACTIVE 0     // Nivel lógico para encender el LED (1=HIGH, 0=LOW)

#define LED_PWM_CHANNEL_R 0
#define LED_PWM_CHANNEL_G 1
#define LED_PWM_CHANNEL_B 2
#define PWM_FREQUENCY_LED 1000      // Hz
#define PWM_BIT_WIDTH_LED 8         // Resolución PWM

#define LED_BRIGHTNESS_MIN 0
#define LED_BRIGHTNESS_MAX 255
#define LED_BRIGHTNESS_DEFAULT 128
#define LED_COLOR_MIN 0
#define LED_COLOR_MAX 255
#define LED_PWM_MAX_DUTY ((1U << PWM_BIT_WIDTH_LED) - 1U)

// --- Battery Monitor ---
#define PIN_BATTERY 39              // ADC pin para lectura de voltaje de batería
#define BATTERY_MULTIPLIER (12.7f / 2.7f)  // Vin_ADC = Vbat * (2.7/12.7) => Vbat = Vin_ADC * (12.7/2.7)

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

/**
 * @brief Secuencia de inicio y disparo
 */
#define SOLENOID_PULSE_DURATION_MS 200
#define SOLENOID_COOLDOWN_MS 40000
#define LAUNCHER_EN2_DELAY_MS 50

/**
 * @brief PowerUpState duration before transitioning to ConfigState
 * Allows power supply to stabilize after launcher activation,
 * preventing brownout conflicts with WiFi peaks
 */
#define POWERUP_STATE_DURATION_MS 1000

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
#define ENABLE_LED_CONTROL 1
#define ENABLE_SOLENOID 1
#define ENABLE_LAUNCHER 1
#define ENABLE_BLE_SCANNER 1
#define ENABLE_BATTERY_MONITOR 1
#define ENABLE_ENVIRONMENT_SENSOR 1
#define ENABLE_MODE_SWITCH 1

#endif // HARDWARE_CONFIG_H

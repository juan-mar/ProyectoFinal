/****************************************************************
 * @file config.h
 * @brief Global firmware configuration flags.
 * This file controls features like debug logging.
 ****************************************************************/

/****************************************************************
 * Include Guards
 ****************************************************************/
#ifndef CONFIG_H
#define CONFIG_H

/****************************************************************
 * Headers
 ****************************************************************/
#include <Arduino.h> 

/****************************************************************
 * Debug Configuration
 ****************************************************************/

/**
 * @brief Set to 1 to enable Serial logging, 0 for release.
 * When set to 0, all LOG_... macros are compiled out,
 * saving flash space and execution time.
 */
#define DEBUG_MODE 1
#define PIN_DEBUG_MODE 1
/****************************************************************
 * Logger Macros (The smart part)
 ****************************************************************/
#if PIN_DEBUG_MODE == 1
  #define PIN_MODE(pin,mode) pinMode(pin,mode)
  #define PIN_HIGH(pin) digitalWrite(pin,HIGH)
  #define PIN_LOW(pin) digitalWrite(pin,LOW)    

#else
  #define PIN_MODE(pin,mode)
  #define PIN_HIGH(pin) 
  #define PIN_LOW(pin)
#endif

#if DEBUG_MODE == 1

  #define LOG_SETUP(baud) Serial.begin(baud)
  #define LOG_PRINT(x) Serial.print(x)
  #define LOG_PRINTLN(x) Serial.println(x)
  #define LOG_PRINTF(f, ...) Serial.printf(f, ##__VA_ARGS__)
  #define LOG_FLUSH() Serial.flush()

#else

  #define LOG_SETUP(baud)
  #define LOG_PRINT(x)
  #define LOG_PRINTLN(x)
  #define LOG_PRINTF(f, ...)
  #define LOG_FLUSH()
#endif // DEBUG_MODE

/****************************************************************
 * Event Logger Configuration
 ****************************************************************/

/**
 * @brief Master switch for Event Logger system
 * Set to 1 to enable event logging, 0 to disable completely
 */
#define EVENT_LOGGER_ENABLED 1

/**
 * @brief Enable LCD I2C output for real-time event display
 * Requires LiquidCrystal_I2C library and physical LCD hardware
 * Set to 0 if no LCD is connected
 */
#define EVENT_LOGGER_LCD_ENABLED 0

/**
 * @brief Enable Web API endpoint /api/logs for remote log viewing
 * Logs can be viewed through the web interface
 */
#define EVENT_LOGGER_WEB_ENABLED 1

/**
 * @brief Size of circular buffer for log storage (in RAM)
 * Larger = more history, but more RAM usage
 * Recommended: 50-150 entries
 */
#define EVENT_LOGGER_BUFFER_SIZE 100

/****************************************************************
 * LCD I2C Configuration (only used if EVENT_LOGGER_LCD_ENABLED == 1)
 ****************************************************************/

/**
 * @brief I2C address of the LCD display
 * Common addresses: 0x27 or 0x3F (check your hardware)
 */
#define EVENT_LCD_ADDR 0x27

/**
 * @brief Number of columns on LCD display
 * Common values: 16 (LCD1602) or 20 (LCD2004)
 */
#define EVENT_LCD_COLS 20

/**
 * @brief Number of rows on LCD display
 * Common values: 2 (LCD1602) or 4 (LCD2004)
 */
#define EVENT_LCD_ROWS 4

// --- Alternative preset for LCD1602 (uncomment to use) ---
// #define EVENT_LCD_COLS 16
// #define EVENT_LCD_ROWS 2

#endif // CONFIG_H
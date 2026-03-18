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
#define DEBUG_MODE 0
#define PIN_DEBUG_MODE 1
#define TEST_LANZAMIENTOS 0
#define TEST_LITTLEFS_CAPACITY 0

/****************************************************************
 * Logger Macros (The smart part)
 ****************************************************************/
#if TEST_LANZAMIENTOS == 1
 #define TEST_LANZAMIENTOS_SOLENOID_INTERVAL_MS (2UL * 60UL * 1000UL)
#define TEST_LANZAMIENTOS_BATTERY_CRITICAL_PERCENT 15
#define TEST_LANZAMIENTOS_BOOT_MARKER_BATTERY 0xBADA5501UL
#define TEST_LANZAMIENTOS_MAX_DISPAROS 60
#endif

#if TEST_LITTLEFS_CAPACITY == 1
#define TEST_LITTLEFS_CAPACITY_MAX_FILES 20
#define TEST_LITTLEFS_CAPACITY_DURATION_S 60
#endif


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

#define EVENT_LOGGER_ENABLED 1
#define EVENT_LOGGER_LCD_ENABLED 1
#define EVENT_LOGGER_WEB_ENABLED 1
#define EVENT_LOGGER_BUFFER_SIZE 180

/****************************************************************
 * LCD I2C Configuration (only used if EVENT_LOGGER_LCD_ENABLED == 1)
 ****************************************************************/
#define EVENT_LCD_ADDR 0x27
#define EVENT_LCD_COLS 20
#define EVENT_LCD_ROWS 4
// --- Alternative preset for LCD1602 (uncomment to use) ---
// #define EVENT_LCD_COLS 16
// #define EVENT_LCD_ROWS 2

#endif // CONFIG_H
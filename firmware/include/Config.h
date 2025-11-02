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

/****************************************************************
 * Logger Macros (The smart part)
 ****************************************************************/

#if DEBUG_MODE == 1

  #define LOG_SETUP(baud) Serial.begin(baud)
  #define LOG_PRINT(x) Serial.print(x)
  #define LOG_PRINTLN(x) Serial.println(x)
  #define LOG_PRINTF(f, ...) Serial.printf(f, ##__VA_ARGS__)

#else

  #define LOG_SETUP(baud)
  #define LOG_PRINT(x)
  #define LOG_PRINTLN(x)
  #define LOG_PRINTF(f, ...)

#endif // DEBUG_MODE

#endif // CONFIG_H
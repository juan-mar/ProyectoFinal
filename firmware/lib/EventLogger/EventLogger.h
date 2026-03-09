/****************************************************************
 * @file EventLogger.h
 * @brief Thread-safe event logging system with LCD and Web output
 * 
 * Features:
 * - Circular buffer in RAM (configurable size)
 * - Thread-safe with mutex protection
 * - Optional LCD I2C output (real-time last N events)
 * - Optional Web API endpoint (JSON format)
 * - Three log levels: INFO, WARN, ERROR
 * - Minimal overhead (~5µs per log call)
 ****************************************************************/

#ifndef EVENT_LOGGER_H
#define EVENT_LOGGER_H

/****************************************************************
 * Headers
 ****************************************************************/
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "Config.h"

#if EVENT_LOGGER_LCD_ENABLED
    #include <LiquidCrystal_I2C.h>
#endif

/****************************************************************
 * Event Log Entry Structure
 ****************************************************************/
struct LogEntry {
    uint32_t timestamp;  // millis()
    uint8_t level;       // 0=INFO, 1=WARN, 2=ERROR
    char message[81];    // Max 80 chars + null terminator
};

/****************************************************************
 * Log Levels
 ****************************************************************/
enum EventLogLevel {
    EVENT_LOG_INFO = 0,
    EVENT_LOG_WARN = 1,
    EVENT_LOG_ERROR = 2
};

/****************************************************************
 * EventLogger Class (Singleton)
 ****************************************************************/
class EventLogger {
public:
    /**
     * @brief Get singleton instance
     */
    static EventLogger* getInstance();

    /**
     * @brief Initialize the logger (call once in setup)
     */
    void begin();

    /**
     * @brief Log an event with specified level
     * @param level Log level (INFO, WARN, ERROR)
     * @param message Message string (max 80 chars, will be truncated)
     */
    void log(EventLogLevel level, const char* message);

    /**
     * @brief Log only if the singleton is available.
     * Resolves getInstance() once and avoids duplicate calls in macros.
     */
    static void logIfAvailable(EventLogLevel level, const char* message);

    /**
     * @brief Update LCD display with latest logs (call from task)
     * This is a blocking operation (~500µs), should be called from dedicated task
     */
    void updateLCD();

    /**
     * @brief Get logs as JSON string for web API
     * @param maxEntries Maximum number of entries to return (0 = all)
     * @return JSON string with log entries
     */
    String getLogsJSON(uint16_t maxEntries = 0);

    /**
     * @brief Get number of logs currently in buffer
     */
    uint16_t getLogCount() const;

    /**
     * @brief Clear all logs from buffer
     */
    void clear();

private:
    // Singleton constructor
    EventLogger();
    ~EventLogger();

    // Prevent copying
    EventLogger(const EventLogger&) = delete;
    EventLogger& operator=(const EventLogger&) = delete;

    // Buffer management
    LogEntry buffer[EVENT_LOGGER_BUFFER_SIZE];
    uint16_t head;   // Next write position
    uint16_t tail;   // Oldest entry (for reading)
    uint16_t count;  // Current number of entries

    // Thread safety
    SemaphoreHandle_t mutex;

    // LCD display (optional)
#if EVENT_LOGGER_LCD_ENABLED
    LiquidCrystal_I2C* lcd;
    uint8_t lastDisplayedIndex[EVENT_LCD_ROWS]; // Track which log is on each LCD row
#endif

    // Helper functions
    void addEntry(const LogEntry& entry);
    LogEntry getEntry(uint16_t index) const;
    const char* getLevelString(EventLogLevel level) const;
    
    // Singleton instance
    static EventLogger* instance;
};

/****************************************************************
 * Convenience Macros
 ****************************************************************/
#if EVENT_LOGGER_ENABLED
    #define EVENT_INFO(msg)  do { EventLogger::logIfAvailable(EVENT_LOG_INFO, (msg)); } while(0)
    #define EVENT_WARN(msg)  do { EventLogger::logIfAvailable(EVENT_LOG_WARN, (msg)); } while(0)
    #define EVENT_ERROR(msg) do { EventLogger::logIfAvailable(EVENT_LOG_ERROR, (msg)); } while(0)
#else
    #define EVENT_INFO(msg)  do {} while(0)
    #define EVENT_WARN(msg)  do {} while(0)
    #define EVENT_ERROR(msg) do {} while(0)
#endif

#endif // EVENT_LOGGER_H

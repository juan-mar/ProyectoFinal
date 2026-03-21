/****************************************************************
 * @file RssiLogger.h
 * @brief Lightweight RSSI signal strength logger for calibration/training
 * 
 * Features:
 * - Captures RSSI values during calibration and training
 * - Small circular buffer in RAM (minimal overhead)
 * - Writes binary records to LittleFS in batches (efficient storage)
 * - Easily disabled via CONFIG_H define for production builds
 * - No separate task needed (non-blocking, ~1µs overhead)
 * 
 * Usage:
 *   RSSI_LOG_CAL(rssiValue);    // Log during calibration
 *   RSSI_LOG_TRAIN(rssiValue);  // Log during training
 * 
 * Commands (via Serial):
 *   '&'  -> Send complete binary dump via UART (framed with RSB1)
 *   '%'  -> Clear log file
 *   '!'  -> Print min/max/avg RSSI
 ****************************************************************/

#ifndef RSSI_LOGGER_H
#define RSSI_LOGGER_H

/****************************************************************
 * Headers
 ****************************************************************/
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "Config.h"

/****************************************************************
 * RSSI Log Entry (minimal footprint)
 ****************************************************************/
struct RssiEntry {
    uint32_t timestamp;  // millis()
    int16_t rssi;        // RSSI value (-120 to 0 dBm)
    uint8_t mode;        // 0=CALIB, 1=TRAIN
};

// Fixed-size binary record stored in LittleFS and streamed over UART.
// Layout: <uint32 millis><int16 rssi><uint8 mode><uint8 reserved>
struct __attribute__((packed)) RssiBinaryRecord {
    uint32_t timestamp;
    int16_t rssi;
    uint8_t mode;
    uint8_t reserved;
};

/****************************************************************
 * Log Modes
 ****************************************************************/
enum RssiLogMode {
    RSSI_MODE_CALIBRATION = 0,
    RSSI_MODE_TRAINING = 1
};

/****************************************************************
 * RssiLogger Class (Singleton)
 ****************************************************************/
class RssiLogger {
public:
    /**
     * @brief Get singleton instance
     */
    static RssiLogger* getInstance();

    /**
     * @brief Initialize the logger (call once in setup)
     */
    void begin();

    /**
     * @brief Log RSSI value for given mode
     * @param mode RSSI_MODE_CALIBRATION or RSSI_MODE_TRAINING
     * @param rssiValue RSSI value in dBm (typically -40 to -120)
     */
    void logRssi(RssiLogMode mode, int16_t rssiValue);

    /**
     * @brief Flush buffer to LittleFS (called periodically or on demand)
     * Thread-safe, non-blocking
     */
    void flush();

    /**
     * @brief Get current number of buffered entries
     */
    uint16_t getBufferedCount() const;

    /**
     * @brief Clear log file from LittleFS
     */
    void clearLogFile();

    /**
    * @brief Load entire binary log from LittleFS and send via Serial.
    * Frame format: [4B magic='RSB1'][4B recSize][4B recCount][payload bytes]
     */
    void dumpLogViaSUART();

    /**
     * @brief Calculate and print statistics (min/max/avg RSSI)
     */
    void printStatistics();

private:
    // Singleton constructor
    RssiLogger();
    ~RssiLogger();

    // Prevent copying
    RssiLogger(const RssiLogger&) = delete;
    RssiLogger& operator=(const RssiLogger&) = delete;

    // Buffer management
    RssiEntry buffer[RSSI_LOGGER_BUFFER_SIZE];
    uint16_t head;    // Next write position
    uint16_t count;   // Current number of entries
    SemaphoreHandle_t mutex;

    // File path (binary format)
    const char* LOG_FILE_PATH = "/rssi_log.bin";

    // UART dump protocol constants
    static constexpr uint8_t DUMP_MAGIC_0 = 'R';
    static constexpr uint8_t DUMP_MAGIC_1 = 'S';
    static constexpr uint8_t DUMP_MAGIC_2 = 'B';
    static constexpr uint8_t DUMP_MAGIC_3 = '1';

    /**
     * @brief Write buffer to LittleFS file (append mode)
     * Must be called with mutex held
     */
    bool writeBufferToFile();

    /**
     * @brief Add entry to buffer with thread safety
     */
    void addEntry(RssiEntry entry);

    // Static singleton instance
    static RssiLogger* instance;
};

/****************************************************************
 * Convenience Macros (compile out if disabled in Config.h)
 ****************************************************************/
#if RSSI_LOGGER_ENABLED == 1
    #define RSSI_LOG_CAL(value) RssiLogger::getInstance()->logRssi(RSSI_MODE_CALIBRATION, (int16_t)(value))
    #define RSSI_LOG_TRAIN(value) RssiLogger::getInstance()->logRssi(RSSI_MODE_TRAINING, (int16_t)(value))
    #define RSSI_FLUSH() RssiLogger::getInstance()->flush()
#else
    #define RSSI_LOG_CAL(value)
    #define RSSI_LOG_TRAIN(value)
    #define RSSI_FLUSH()
#endif

#endif // RSSI_LOGGER_H

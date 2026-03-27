/****************************************************************
 * @file RssiLogger.h
 * @brief Lightweight RSSI signal strength logger for calibration/training (UART/RAM only)
 * 
 * Features:
 * - Captures RSSI values during calibration and training
 * - Small circular buffer in RAM (minimal overhead)
 * - Streams data over UART/I2C transport
 * - Easily disabled via CONFIG_H define for production builds
 * - No separate task needed (non-blocking, ~1µs overhead)
 * 
 * Usage:
 *   RSSI_LOG_CAL(rssiValue);    // Log during calibration
 *   RSSI_LOG_TRAIN(rssiValue);  // Log during training
 * 
 * Commands (via Serial):
 *   '&'  -> Send RAM buffer dump via UART (framed with RSB1)
 *   '%'  -> Clear RAM buffer
 *   '!'  -> Print min/max/avg RSSI from RAM buffer
 ****************************************************************/

#ifndef RSSI_LOGGER_H
#define RSSI_LOGGER_H

/****************************************************************
 * Headers
 ****************************************************************/
#include <Arduino.h>
#include <string.h>
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

// Fixed-size binary record streamed over UART.
// Layout: <uint32 millis><int16 rssi><uint8 mode><uint8 reserved>
struct __attribute__((packed)) RssiBinaryRecord {
    uint32_t timestamp;
    int16_t rssi;
    uint8_t mode;
    uint8_t reserved;
};

struct UartDatEntry {
    uint32_t timestamp;
    char tipo[8];
    float rssiT1;
    float cmpT1;
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
     * @brief Get current number of buffered entries
     */
    uint16_t getBufferedCount() const;

    /**
        * @brief Clear RAM buffer
     */
    void clearLogFile();

    /**
        * @brief Load entire buffered binary log from RAM and send via Serial.
    * Frame format: [4B magic='RSB1'][4B recSize][4B recCount][payload bytes]
     */
    void dumpLogViaSUART();

    /**
     * @brief Calculate and print statistics (min/max/avg RSSI)
     */
    void printStatistics();

    /**
     * @brief Emit one positional DAT line over UART.
     * Format: DAT,<millis>,<tipo>,<rssi_t1>,<cmp_t1>
     */
    void emitUartDAT(uint32_t tsMillis, const char* tipo, float rssiT1, float cmpT1);

    /**
     * @brief Enqueue one DAT line for deferred UART emission from task context.
     * @return true if queued, false if queue full or unavailable.
     */
    bool enqueueUartDAT(uint32_t tsMillis, const char* tipo, float rssiT1, float cmpT1);

    /**
     * @brief Flush up to maxItems queued DAT entries through UART.
     * @return Number of entries emitted.
     */
    uint16_t flushQueuedUartDAT(uint16_t maxItems);

    /**
     * @brief Emit one positional EVT line over UART.
     * Format: EVT,<millis>,<state>
     */
    void emitUartEVT(uint32_t tsMillis, const char* state);

    /**
     * @brief Emit one positional CFG line over UART.
     * Format: CFG,<millis>,<q>,<r>,<x0>,<p0>,<media_calib>,<varianza_calib>,<histeresis_in>,<histeresis_out>[,<estado_inicial>]
     */
    void emitUartCFG(
        uint32_t tsMillis,
        float kalmanQ,
        float kalmanR,
        float kalmanX0,
        float kalmanP0,
        float mediaCalib,
        float varianzaCalib,
        float histeresisIn,
        float histeresisOut,
        const char* estadoInicial = nullptr
    );

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

    // DAT UART deferred queue (for callback-safe telemetry)
    UartDatEntry datQueue[RSSI_LOGGER_UART_DAT_QUEUE_SIZE];
    uint16_t datQHead;
    uint16_t datQTail;
    uint16_t datQCount;
    SemaphoreHandle_t datQueueMutex;

    // UART dump protocol constants
    static constexpr uint8_t DUMP_MAGIC_0 = 'R';
    static constexpr uint8_t DUMP_MAGIC_1 = 'S';
    static constexpr uint8_t DUMP_MAGIC_2 = 'B';
    static constexpr uint8_t DUMP_MAGIC_3 = '1';

    /**
     * @brief Add entry to buffer with thread safety
     */
    void addEntry(RssiEntry entry);

    /**
     * @brief Emit one live sample through configured transport (UART or I2C)
     */
    void emitLiveSample(const RssiEntry& entry);

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
    #define RSSI_UART_DAT(tipo, rssi_t1, cmp_t1) \
        RssiLogger::getInstance()->enqueueUartDAT(millis(), (tipo), (float)(rssi_t1), (float)(cmp_t1))
    #define RSSI_UART_POLL_DAT(max_items) \
        RssiLogger::getInstance()->flushQueuedUartDAT((uint16_t)(max_items))
    #define RSSI_UART_EVT(state) \
        RssiLogger::getInstance()->emitUartEVT(millis(), (state))
    #define RSSI_UART_CFG(kalman_q, kalman_r, kalman_x0, kalman_p0, media_calib, varianza_calib, histeresis_in, histeresis_out, estado_inicial) \
        RssiLogger::getInstance()->emitUartCFG( \
            millis(), \
            (float)(kalman_q), \
            (float)(kalman_r), \
            (float)(kalman_x0), \
            (float)(kalman_p0), \
            (float)(media_calib), \
            (float)(varianza_calib), \
            (float)(histeresis_in), \
            (float)(histeresis_out), \
            (estado_inicial) \
        )
#else
    #define RSSI_LOG_CAL(value)
    #define RSSI_LOG_TRAIN(value)
    #define RSSI_FLUSH()
    #define RSSI_UART_DAT(tipo, rssi_t1, cmp_t1)
    #define RSSI_UART_POLL_DAT(max_items)
    #define RSSI_UART_EVT(state)
    #define RSSI_UART_CFG(kalman_q, kalman_r, kalman_x0, kalman_p0, media_calib, varianza_calib, histeresis_in, histeresis_out, estado_inicial)
#endif

#endif // RSSI_LOGGER_H

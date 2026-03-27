/****************************************************************
 * @file RssiLogger.cpp
 * @brief Implementation of RssiLogger class
 ****************************************************************/

#include "RssiLogger.h"

#if RSSI_LOGGER_ENABLED == 1

#if RSSI_LOGGER_TRANSPORT == RSSI_LOGGER_TRANSPORT_I2C
#include <Wire.h>
#endif

/****************************************************************
 * Static Members
 ****************************************************************/
RssiLogger* RssiLogger::instance = nullptr;

/****************************************************************
 * Public Methods
 ****************************************************************/

RssiLogger* RssiLogger::getInstance() {
    if (instance == nullptr) {
        instance = new RssiLogger();
    }
    return instance;
}

RssiLogger::RssiLogger() 
    : head(0), count(0), mutex(nullptr),
      datQHead(0), datQTail(0), datQCount(0), datQueueMutex(nullptr) {
    // Create mutex for thread safety
    mutex = xSemaphoreCreateMutex();
    datQueueMutex = xSemaphoreCreateMutex();
}

RssiLogger::~RssiLogger() {
    if (mutex != nullptr) {
        vSemaphoreDelete(mutex);
    }
    if (datQueueMutex != nullptr) {
        vSemaphoreDelete(datQueueMutex);
    }
}

void RssiLogger::begin() {
#if RSSI_LOGGER_TRANSPORT == RSSI_LOGGER_TRANSPORT_UART
#if DEBUG_MODE == 0
    // Ensure UART is initialized even when debug logging is disabled.
    Serial.begin(RSSI_LOGGER_UART_BAUD);
#endif
    LOG_PRINTF("[RssiLogger] Live transport: UART @ %lu\n", (unsigned long)RSSI_LOGGER_UART_BAUD);
#elif RSSI_LOGGER_TRANSPORT == RSSI_LOGGER_TRANSPORT_I2C
    // This device acts as I2C master and pushes one packet per sample.
    Wire.begin(RSSI_LOGGER_I2C_SDA, RSSI_LOGGER_I2C_SCL, RSSI_LOGGER_I2C_FREQ_HZ);
    LOG_PRINTF("[RssiLogger] Live transport: I2C addr=0x%02X\n", RSSI_LOGGER_I2C_ADDR);
#else
    LOG_PRINTLN("[RssiLogger] Live transport: NONE");
#endif

    LOG_PRINTLN("[RssiLogger] Initialized in RAM/UART mode (no LittleFS)");
}

void RssiLogger::logRssi(RssiLogMode mode, int16_t rssiValue) {
    if (mutex == nullptr) {
        return;
    }

    RssiEntry entry;
    entry.timestamp = millis();
    entry.rssi = rssiValue;
    entry.mode = static_cast<uint8_t>(mode);

    // Optional legacy sample stream (millis,mode,rssi).
#if RSSI_LOGGER_LEGACY_LIVE_SAMPLE_ENABLED == 1
    emitLiveSample(entry);
#endif

    // Keep a compact in-memory history for optional dump/stats commands.
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        addEntry(entry);
        xSemaphoreGive(mutex);
    }
}

bool RssiLogger::enqueueUartDAT(uint32_t tsMillis, const char* tipo, float rssiT1, float cmpT1) {
    if (datQueueMutex == nullptr) {
        return false;
    }

    if (xSemaphoreTake(datQueueMutex, 0) != pdTRUE) {
        return false;
    }

    if (datQCount >= RSSI_LOGGER_UART_DAT_QUEUE_SIZE) {
        xSemaphoreGive(datQueueMutex);
        return false;
    }

    UartDatEntry& slot = datQueue[datQHead];
    slot.timestamp = tsMillis;
    const char* resolvedTipo = (tipo != nullptr) ? tipo : "UNK";
    strncpy(slot.tipo, resolvedTipo, sizeof(slot.tipo) - 1U);
    slot.tipo[sizeof(slot.tipo) - 1U] = '\0';
    slot.rssiT1 = rssiT1;
    slot.cmpT1 = cmpT1;

    datQHead = (uint16_t)((datQHead + 1U) % RSSI_LOGGER_UART_DAT_QUEUE_SIZE);
    datQCount++;
    xSemaphoreGive(datQueueMutex);
    return true;
}

uint16_t RssiLogger::flushQueuedUartDAT(uint16_t maxItems) {
#if RSSI_LOGGER_TRANSPORT != RSSI_LOGGER_TRANSPORT_UART
    (void)maxItems;
    return 0;
#else
    if (datQueueMutex == nullptr || maxItems == 0U) {
        return 0;
    }

    uint16_t sent = 0;
    while (sent < maxItems) {
        UartDatEntry item;
        bool hasItem = false;

        if (xSemaphoreTake(datQueueMutex, pdMS_TO_TICKS(1)) == pdTRUE) {
            if (datQCount > 0U) {
                item = datQueue[datQTail];
                datQTail = (uint16_t)((datQTail + 1U) % RSSI_LOGGER_UART_DAT_QUEUE_SIZE);
                datQCount--;
                hasItem = true;
            }
            xSemaphoreGive(datQueueMutex);
        }

        if (!hasItem) {
            break;
        }

        emitUartDAT(item.timestamp, item.tipo, item.rssiT1, item.cmpT1);
        sent++;
    }

    return sent;
#endif
}

void RssiLogger::emitLiveSample(const RssiEntry& entry) {
#if RSSI_LOGGER_TRANSPORT == RSSI_LOGGER_TRANSPORT_UART
    // ASCII stream for easy Python parsing: millis,mode,rssi
    Serial.printf("%lu,%u,%d\n",
                  (unsigned long)entry.timestamp,
                  (unsigned int)entry.mode,
                  (int)entry.rssi);
#elif RSSI_LOGGER_TRANSPORT == RSSI_LOGGER_TRANSPORT_I2C
    // Binary packet over I2C: 8-byte record
    RssiBinaryRecord rec;
    rec.timestamp = entry.timestamp;
    rec.rssi = entry.rssi;
    rec.mode = entry.mode;
    rec.reserved = 0;

    Wire.beginTransmission(RSSI_LOGGER_I2C_ADDR);
    Wire.write(reinterpret_cast<const uint8_t*>(&rec), sizeof(rec));
    (void)Wire.endTransmission();
#else
    (void)entry;
#endif
}

void RssiLogger::emitUartDAT(uint32_t tsMillis, const char* tipo, float rssiT1, float cmpT1) {
#if RSSI_LOGGER_TRANSPORT == RSSI_LOGGER_TRANSPORT_UART
    if (tipo == nullptr) {
        tipo = "UNK";
    }
    Serial.printf("DAT,%lu,%s,%.3f,%.3f\n",
                  (unsigned long)tsMillis,
                  tipo,
                  (double)rssiT1,
                  (double)cmpT1);
#else
    (void)tsMillis;
    (void)tipo;
    (void)rssiT1;
    (void)cmpT1;
#endif
}

void RssiLogger::emitUartEVT(uint32_t tsMillis, const char* state) {
#if RSSI_LOGGER_TRANSPORT == RSSI_LOGGER_TRANSPORT_UART
    if (state == nullptr) {
        state = "UNK";
    }
    Serial.printf("EVT,%lu,%s\n", (unsigned long)tsMillis, state);
#else
    (void)tsMillis;
    (void)state;
#endif
}

void RssiLogger::emitUartCFG(
    uint32_t tsMillis,
    float kalmanQ,
    float kalmanR,
    float kalmanX0,
    float kalmanP0,
    float mediaCalib,
    float varianzaCalib,
    float histeresisIn,
    float histeresisOut,
    const char* estadoInicial
) {
#if RSSI_LOGGER_TRANSPORT == RSSI_LOGGER_TRANSPORT_UART
    if (estadoInicial == nullptr || estadoInicial[0] == '\0') {
        Serial.printf(
            "CFG,%lu,%.6f,%.6f,%.6f,%.6f,%.3f,%.3f,%.3f,%.3f\n",
            (unsigned long)tsMillis,
            (double)kalmanQ,
            (double)kalmanR,
            (double)kalmanX0,
            (double)kalmanP0,
            (double)mediaCalib,
            (double)varianzaCalib,
            (double)histeresisIn,
            (double)histeresisOut
        );
    } else {
        Serial.printf(
            "CFG,%lu,%.6f,%.6f,%.6f,%.6f,%.3f,%.3f,%.3f,%.3f,%s\n",
            (unsigned long)tsMillis,
            (double)kalmanQ,
            (double)kalmanR,
            (double)kalmanX0,
            (double)kalmanP0,
            (double)mediaCalib,
            (double)varianzaCalib,
            (double)histeresisIn,
            (double)histeresisOut,
            estadoInicial
        );
    }
#else
    (void)tsMillis;
    (void)kalmanQ;
    (void)kalmanR;
    (void)kalmanX0;
    (void)kalmanP0;
    (void)mediaCalib;
    (void)varianzaCalib;
    (void)histeresisIn;
    (void)histeresisOut;
    (void)estadoInicial;
#endif
}

void RssiLogger::addEntry(RssiEntry entry) {
    // Simple circular buffer management
    buffer[head] = entry;
    head = (head + 1) % RSSI_LOGGER_BUFFER_SIZE;
    
    if (count < RSSI_LOGGER_BUFFER_SIZE) {
        count++;
    }
}

uint16_t RssiLogger::getBufferedCount() const {
    return count;
}

void RssiLogger::clearLogFile() {
    if (mutex == nullptr) {
        return;
    }
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        head = 0;
        count = 0;
        xSemaphoreGive(mutex);
    }
    LOG_PRINTLN("[RssiLogger] RAM buffer cleared");
}

void RssiLogger::dumpLogViaSUART() {
#if RSSI_LOGGER_DUMP_ENABLED != 1
    LOG_PRINTLN("[RssiLogger] Dump disabled by config");
    return;
#else
    if (mutex == nullptr) {
        return;
    }

    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        LOG_PRINTLN("[RssiLogger] ERROR: dump lock timeout");
        return;
    }
    uint32_t recordSize = sizeof(RssiBinaryRecord);
    uint32_t recordCount = static_cast<uint32_t>(count);

    // Frame header for Python parser:
    // [4B magic='RSB1'][4B little-endian recordSize][4B little-endian recordCount]
    uint8_t header[12];
    header[0] = DUMP_MAGIC_0;
    header[1] = DUMP_MAGIC_1;
    header[2] = DUMP_MAGIC_2;
    header[3] = DUMP_MAGIC_3;
    header[4] = static_cast<uint8_t>(recordSize & 0xFFU);
    header[5] = static_cast<uint8_t>((recordSize >> 8U) & 0xFFU);
    header[6] = static_cast<uint8_t>((recordSize >> 16U) & 0xFFU);
    header[7] = static_cast<uint8_t>((recordSize >> 24U) & 0xFFU);
    header[8] = static_cast<uint8_t>(recordCount & 0xFFU);
    header[9] = static_cast<uint8_t>((recordCount >> 8U) & 0xFFU);
    header[10] = static_cast<uint8_t>((recordCount >> 16U) & 0xFFU);
    header[11] = static_cast<uint8_t>((recordCount >> 24U) & 0xFFU);
    Serial.write(header, sizeof(header));

    uint16_t tail = (head >= count) ? (head - count) : (RSSI_LOGGER_BUFFER_SIZE - count + head);
    for (uint16_t i = 0; i < count; i++) {
        uint16_t idx = (tail + i) % RSSI_LOGGER_BUFFER_SIZE;
        const RssiEntry& e = buffer[idx];

        RssiBinaryRecord rec;
        rec.timestamp = e.timestamp;
        rec.rssi = e.rssi;
        rec.mode = e.mode;
        rec.reserved = 0;
        Serial.write(reinterpret_cast<const uint8_t*>(&rec), sizeof(rec));
    }

    xSemaphoreGive(mutex);
#endif
}

void RssiLogger::printStatistics() {
    if (mutex == nullptr) {
        return;
    }

    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        LOG_PRINTLN("[RssiLogger] ERROR: stats lock timeout");
        return;
    }

    int16_t minRssi = 0;
    int16_t maxRssi = -120;
    int32_t sumRssi = 0;
    uint32_t sampleCount = 0;
    uint32_t calibCount = 0;
    uint32_t trainCount = 0;
    bool initialized = false;

    uint16_t localCount = this->count;
    uint16_t tail = (head >= localCount) ? (head - localCount) : (RSSI_LOGGER_BUFFER_SIZE - localCount + head);

    for (uint16_t i = 0; i < localCount; i++) {
        uint16_t idx = (tail + i) % RSSI_LOGGER_BUFFER_SIZE;
        const RssiEntry& rec = buffer[idx];

        if (!initialized) {
            minRssi = rec.rssi;
            maxRssi = rec.rssi;
            initialized = true;
        } else {
            minRssi = min(minRssi, rec.rssi);
            maxRssi = max(maxRssi, rec.rssi);
        }

        sumRssi += rec.rssi;
        sampleCount++;

        if (rec.mode == RSSI_MODE_CALIBRATION) {
            calibCount++;
        } else if (rec.mode == RSSI_MODE_TRAINING) {
            trainCount++;
        }
    }
    xSemaphoreGive(mutex);

    if (sampleCount > 0) {
        int16_t avgRssi = (int16_t)(sumRssi / sampleCount);
        
        LOG_PRINTLN("\n=== RSSI STATISTICS ===");
        LOG_PRINTF("Total entries: %lu\n", sampleCount);
        LOG_PRINTF("Calibration: %lu | Training: %lu\n", calibCount, trainCount);
        LOG_PRINTF("Min RSSI:  %d dBm\n", minRssi);
        LOG_PRINTF("Max RSSI:  %d dBm\n", maxRssi);
        LOG_PRINTF("Avg RSSI:  %d dBm\n", avgRssi);
        LOG_PRINTLN("========================\n");
    } else {
        LOG_PRINTLN("[RssiLogger] No data in RAM buffer");
    }
}

#endif  // RSSI_LOGGER_ENABLED == 1

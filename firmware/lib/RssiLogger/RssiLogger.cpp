/****************************************************************
 * @file RssiLogger.cpp
 * @brief Implementation of RssiLogger class
 ****************************************************************/

#include "RssiLogger.h"

#if RSSI_LOGGER_ENABLED == 1

#include <LittleFS.h>

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
    : head(0), count(0), mutex(nullptr) {
    // Create mutex for thread safety
    mutex = xSemaphoreCreateMutex();
}

RssiLogger::~RssiLogger() {
    if (mutex != nullptr) {
        vSemaphoreDelete(mutex);
    }
}

void RssiLogger::begin() {
    // Ensure LittleFS is initialized (should already be done in DataManager)
    if (!LittleFS.begin()) {
        LOG_PRINTLN("[RssiLogger] ERROR: LittleFS not initialized!");
        return;
    }
    
    LOG_PRINTLN("[RssiLogger] Initialized. File: /rssi_log.bin");

    // Ensure binary log file exists.
    if (!LittleFS.exists(LOG_FILE_PATH)) {
        File file = LittleFS.open(LOG_FILE_PATH, "w");
        if (file) {
            file.close();
            LOG_PRINTLN("[RssiLogger] Created empty binary log file");
        }
    } else {
        File file = LittleFS.open(LOG_FILE_PATH, "r");
        if (file) {
            LOG_PRINTF("[RssiLogger] Existing binary log file found. Size: %u bytes\n",
                       (unsigned int)file.size());
            file.close();
        }
    }
}

void RssiLogger::logRssi(RssiLogMode mode, int16_t rssiValue) {
    if (mutex == nullptr) {
        return;
    }

    RssiEntry entry;
    entry.timestamp = millis();
    entry.rssi = rssiValue;
    entry.mode = static_cast<uint8_t>(mode);

    // Thread-safe buffer write
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        addEntry(entry);
        
        // Auto-flush every N entries to balance memory vs I/O
        if (count >= RSSI_LOGGER_BUFFER_SIZE) {
            writeBufferToFile();
            count = 0;
            head = 0;
        }
        
        xSemaphoreGive(mutex);
    }
}

void RssiLogger::addEntry(RssiEntry entry) {
    // Simple circular buffer management
    buffer[head] = entry;
    head = (head + 1) % RSSI_LOGGER_BUFFER_SIZE;
    
    if (count < RSSI_LOGGER_BUFFER_SIZE) {
        count++;
    }
}

void RssiLogger::flush() {
    if (mutex == nullptr) {
        return;
    }

    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (count > 0) {
            writeBufferToFile();
            count = 0;
            head = 0;
        }
        xSemaphoreGive(mutex);
    }
}

bool RssiLogger::writeBufferToFile() {
    // Must be called with mutex held
    File file = LittleFS.open(LOG_FILE_PATH, "a");  // Append mode
    if (!file) {
        LOG_PRINTLN("[RssiLogger] ERROR: Cannot open log file for writing");
        return false;
    }

    uint16_t tail = (head > count) ? (head - count) : (RSSI_LOGGER_BUFFER_SIZE - count + head);
    
    for (uint16_t i = 0; i < count; i++) {
        uint16_t idx = (tail + i) % RSSI_LOGGER_BUFFER_SIZE;
        RssiEntry& e = buffer[idx];

        RssiBinaryRecord rec;
        rec.timestamp = e.timestamp;
        rec.rssi = e.rssi;
        rec.mode = e.mode;
        rec.reserved = 0;

        size_t written = file.write(reinterpret_cast<const uint8_t*>(&rec), sizeof(rec));
        if (written != sizeof(rec)) {
            file.close();
            LOG_PRINTLN("[RssiLogger] ERROR: Partial binary write");
            return false;
        }
    }

    file.close();
    LOG_PRINTF("[RssiLogger] Flushed %u entries to file\n", count);
    return true;
}

uint16_t RssiLogger::getBufferedCount() const {
    return count;
}

void RssiLogger::clearLogFile() {
    if (LittleFS.remove(LOG_FILE_PATH)) {
        LOG_PRINTLN("[RssiLogger] Log file cleared");

        // Recreate empty binary file
        File file = LittleFS.open(LOG_FILE_PATH, "w");
        if (file) {
            file.close();
        }
    } else {
        LOG_PRINTLN("[RssiLogger] ERROR: Cannot clear log file");
    }
}

void RssiLogger::dumpLogViaSUART() {
    // Flush any pending entries first
    flush();

    if (!LittleFS.exists(LOG_FILE_PATH)) {
        LOG_PRINTLN("[RssiLogger] ERROR: Log file does not exist");
        return;
    }

    File file = LittleFS.open(LOG_FILE_PATH, "r");
    if (!file) {
        LOG_PRINTLN("[RssiLogger] ERROR: Cannot open log file for reading");
        return;
    }

    size_t fileSize = file.size();
    uint32_t recordSize = sizeof(RssiBinaryRecord);
    uint32_t recordCount = static_cast<uint32_t>(fileSize / recordSize);

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

    // Send binary payload in chunks
    uint8_t chunk[256];
    while (file.available()) {
        size_t toRead = file.available();
        if (toRead > sizeof(chunk)) {
            toRead = sizeof(chunk);
        }
        size_t bytesRead = file.read(chunk, toRead);
        if (bytesRead == 0) {
            break;
        }
        Serial.write(chunk, bytesRead);
    }

    file.close();
}

void RssiLogger::printStatistics() {
    flush();

    if (!LittleFS.exists(LOG_FILE_PATH)) {
        LOG_PRINTLN("[RssiLogger] ERROR: Log file does not exist");
        return;
    }

    File file = LittleFS.open(LOG_FILE_PATH, "r");
    if (!file) {
        LOG_PRINTLN("[RssiLogger] ERROR: Cannot open log file");
        return;
    }

    int16_t minRssi = 0;
    int16_t maxRssi = -120;
    int32_t sumRssi = 0;
    uint32_t count = 0;
    uint32_t calibCount = 0;
    uint32_t trainCount = 0;
    bool initialized = false;

    while (file.available()) {
        RssiBinaryRecord rec;
        size_t bytesRead = file.read(reinterpret_cast<uint8_t*>(&rec), sizeof(rec));
        if (bytesRead != sizeof(rec)) {
            break;
        }

        if (!initialized) {
            minRssi = rec.rssi;
            maxRssi = rec.rssi;
            initialized = true;
        } else {
            minRssi = min(minRssi, rec.rssi);
            maxRssi = max(maxRssi, rec.rssi);
        }

        sumRssi += rec.rssi;
        count++;

        if (rec.mode == RSSI_MODE_CALIBRATION) {
            calibCount++;
        } else if (rec.mode == RSSI_MODE_TRAINING) {
            trainCount++;
        }
    }

    file.close();

    if (count > 0) {
        int16_t avgRssi = (int16_t)(sumRssi / count);
        
        LOG_PRINTLN("\n=== RSSI STATISTICS ===");
        LOG_PRINTF("Total entries: %lu\n", count);
        LOG_PRINTF("Calibration: %lu | Training: %lu\n", calibCount, trainCount);
        LOG_PRINTF("Min RSSI:  %d dBm\n", minRssi);
        LOG_PRINTF("Max RSSI:  %d dBm\n", maxRssi);
        LOG_PRINTF("Avg RSSI:  %d dBm\n", avgRssi);
        LOG_PRINTLN("========================\n");
    } else {
        LOG_PRINTLN("[RssiLogger] No data in log file");
    }
}

#endif  // RSSI_LOGGER_ENABLED == 1

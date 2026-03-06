/****************************************************************
 * @file EventLogger.cpp
 * @brief Implementation of EventLogger class
 ****************************************************************/

#include "EventLogger.h"

/****************************************************************
 * Static Members
 ****************************************************************/
EventLogger* EventLogger::instance = nullptr;

/****************************************************************
 * Public Methods
 ****************************************************************/

EventLogger* EventLogger::getInstance() {
    if (instance == nullptr) {
        instance = new EventLogger();
    }
    return instance;
}

EventLogger::EventLogger() 
    : head(0), tail(0), count(0), mutex(nullptr)
#if EVENT_LOGGER_LCD_ENABLED
    , lcd(nullptr)
#endif
{
    // Create mutex for thread safety
    mutex = xSemaphoreCreateMutex();
    
#if EVENT_LOGGER_LCD_ENABLED
    // Initialize last displayed indices
    for (uint8_t i = 0; i < EVENT_LCD_ROWS; i++) {
        lastDisplayedIndex[i] = 0xFF; // Invalid index
    }
#endif
}

EventLogger::~EventLogger() {
    if (mutex != nullptr) {
        vSemaphoreDelete(mutex);
    }
#if EVENT_LOGGER_LCD_ENABLED
    if (lcd != nullptr) {
        delete lcd;
    }
#endif
}

void EventLogger::begin() {
#if EVENT_LOGGER_LCD_ENABLED
    // Initialize LCD
    lcd = new LiquidCrystal_I2C(EVENT_LCD_ADDR, EVENT_LCD_COLS, EVENT_LCD_ROWS);
    lcd->init();
    lcd->backlight();
    lcd->clear();
    
    // Show startup message
    lcd->setCursor(0, 0);
    lcd->print("Event Logger");
    lcd->setCursor(0, 1);
    lcd->print("Initialized");
    vTaskDelay(pdMS_TO_TICKS(1000)); // Show for 1 second
    lcd->clear();
#endif

    LOG_PRINTLN("[EventLogger] Initialized");
}

void EventLogger::log(EventLogLevel level, const char* message) {
    if (message == nullptr || mutex == nullptr) {
        return;
    }

    // Create log entry
    LogEntry entry;
    entry.timestamp = millis();
    entry.level = static_cast<uint8_t>(level);
    
    // Copy message (truncate if too long)
    strncpy(entry.message, message, 80);
    entry.message[80] = '\0'; // Ensure null termination

    // Thread-safe buffer write
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        addEntry(entry);
        xSemaphoreGive(mutex);
    }
}

void EventLogger::updateLCD() {
#if EVENT_LOGGER_LCD_ENABLED
    if (lcd == nullptr || mutex == nullptr) {
        return;
    }

    // Get latest logs (thread-safe)
    LogEntry entries[EVENT_LCD_ROWS];
    uint8_t entriesToShow = 0;

    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        // Get last N entries (where N = LCD rows)
        uint16_t startIdx = (count > EVENT_LCD_ROWS) ? (count - EVENT_LCD_ROWS) : 0;
        
        for (uint16_t i = 0; i < EVENT_LCD_ROWS && (startIdx + i) < count; i++) {
            entries[i] = getEntry(startIdx + i);
            entriesToShow++;
        }
        
        xSemaphoreGive(mutex);
    }

    // Update LCD display
    lcd->clear();
    for (uint8_t row = 0; row < entriesToShow; row++) {
        lcd->setCursor(0, row);
        
        // Format: "L MSG..."
        // L = level indicator (I/W/E)
        // MSG = message (truncated to fit)
        
        char rowText[EVENT_LCD_COLS + 1];
        char levelChar = 'I';
        
        switch (entries[row].level) {
            case EVENT_LOG_INFO:  levelChar = 'I'; break;
            case EVENT_LOG_WARN:  levelChar = 'W'; break;
            case EVENT_LOG_ERROR: levelChar = 'E'; break;
        }
        
        // Format: "I: Message text here..."
        snprintf(rowText, EVENT_LCD_COLS + 1, "%c:%s", levelChar, entries[row].message);
        rowText[EVENT_LCD_COLS] = '\0'; // Ensure it fits
        
        lcd->print(rowText);
    }
#endif
}

String EventLogger::getLogsJSON(uint16_t maxEntries) {
    if (mutex == nullptr) {
        return "[]";
    }

    String json = "[";
    
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        uint16_t entriesToReturn = count;
        if (maxEntries > 0 && maxEntries < count) {
            entriesToReturn = maxEntries;
        }
        
        uint16_t startIdx = (entriesToReturn < count) ? (count - entriesToReturn) : 0;
        
        for (uint16_t i = 0; i < entriesToReturn; i++) {
            LogEntry entry = getEntry(startIdx + i);
            
            if (i > 0) {
                json += ",";
            }
            
            json += "{";
            json += "\"timestamp\":";
            json += entry.timestamp;
            json += ",\"level\":\"";
            json += getLevelString(static_cast<EventLogLevel>(entry.level));
            json += "\",\"message\":\"";
            
            // Escape special characters in message
            for (int j = 0; entry.message[j] != '\0' && j < 80; j++) {
                char c = entry.message[j];
                if (c == '"' || c == '\\') {
                    json += '\\';
                }
                json += c;
            }
            
            json += "\"}";
        }
        
        xSemaphoreGive(mutex);
    }
    
    json += "]";
    return json;
}

uint16_t EventLogger::getLogCount() const {
    return count;
}

void EventLogger::clear() {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        head = 0;
        tail = 0;
        count = 0;
        xSemaphoreGive(mutex);
    }
    
#if EVENT_LOGGER_LCD_ENABLED
    if (lcd != nullptr) {
        lcd->clear();
    }
#endif
}

/****************************************************************
 * Private Methods
 ****************************************************************/

void EventLogger::addEntry(const LogEntry& entry) {
    // Add to circular buffer
    buffer[head] = entry;
    head = (head + 1) % EVENT_LOGGER_BUFFER_SIZE;
    
    if (count < EVENT_LOGGER_BUFFER_SIZE) {
        count++;
    } else {
        // Buffer is full, move tail forward (overwrite oldest)
        tail = (tail + 1) % EVENT_LOGGER_BUFFER_SIZE;
    }
}

LogEntry EventLogger::getEntry(uint16_t index) const {
    // index is relative to tail (0 = oldest, count-1 = newest)
    uint16_t actualIndex = (tail + index) % EVENT_LOGGER_BUFFER_SIZE;
    return buffer[actualIndex];
}

const char* EventLogger::getLevelString(EventLogLevel level) const {
    switch (level) {
        case EVENT_LOG_INFO:  return "INFO";
        case EVENT_LOG_WARN:  return "WARN";
        case EVENT_LOG_ERROR: return "ERROR";
        default:              return "UNKNOWN";
    }
}

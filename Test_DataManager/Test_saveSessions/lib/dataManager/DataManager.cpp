/****************************************************************
 * @file DataManager.cpp
 * @brief Implements the DataManager class methods.
 ****************************************************************/

/****************************************************************
 * Headers
 ****************************************************************/
#include "DataManager.h"
#include "config.h" // For logging

/****************************************************************
 * Defines
 ****************************************************************/
// NVS Keys (max 15 chars)
#define KEY_DEVICE_ID "dev_id"
#define KEY_WIFI_SSID "wifi_ssid"
#define KEY_WIFI_PASS "wifi_pass"

/****************************************************************
 * Class Method Implementations
 ****************************************************************/

DataManager::DataManager() : isInitialized(false){
    storageMutex = xSemaphoreCreateMutex();
    if (storageMutex == NULL) {
        LOG_PRINTLN("FATAL: Failed to create storageMutex!");
    }   
}

DataManager::~DataManager() {
    LOG_PRINTLN("DataManager: Destructor called.");
    
    if (storageMutex != NULL) {
        vSemaphoreDelete(storageMutex);
    }
}

bool DataManager::init() {
LOG_PRINTLN("Initializing DataManager...");
    if (isInitialized) {
        return true;
    }

    if (storageMutex == NULL) {
        LOG_PRINTLN("FATAL: Failed to create storageMutex!");
        return false;
    }
    
    // 2. Montar LittleFS
    if (!LittleFS.begin()) {
        LOG_PRINTLN("Failed to mount LittleFS! Formatting...");
        if (LittleFS.format()) {
            LOG_PRINTLN("LittleFS formatted successfully.");
            return LittleFS.begin();
        } else {
            LOG_PRINTLN("FATAL: Failed to format LittleFS.");
            return false;
        }
    }
    LOG_PRINTLN("LittleFS mounted.");
    
    if (xSemaphoreTake(storageMutex, portMAX_DELAY) == pdTRUE) {
        if (!LittleFS.exists(DIR_SESSIONS)) {
            LOG_PRINTLN("Session directory not found, creating...");
            LittleFS.mkdir(DIR_SESSIONS);
        }
        xSemaphoreGive(storageMutex);
    }
    
    isInitialized = true;
    return true;
}

int DataManager::countPendingSessions() {
    int count = 0;
    LOG_PRINTLN("DataManager: Counting session files...");

    if (xSemaphoreTake(storageMutex, portMAX_DELAY) == pdTRUE) {
        
        File root = LittleFS.open(DIR_SESSIONS, "r");
        if (root) {
            File file = root.openNextFile();
            while (file) {
                // Nos aseguramos de que no sea un directorio
                if (!file.isDirectory()) {
                    count++;
                }
                file.close();
                file = root.openNextFile();
            }
            root.close();
        } else {
            LOG_PRINTLN("DataManager: Failed to open session directory.");
        }
        
        xSemaphoreGive(storageMutex);
    }
    
    LOG_PRINTF("DataManager: Found %d pending sessions.\n", count);
    return count;
}

void DataManager::getStorageUsage(size_t &totalBytes, size_t &usedBytes) {
    if (xSemaphoreTake(storageMutex, portMAX_DELAY) == pdTRUE) {
        totalBytes = LittleFS.totalBytes();
        usedBytes = LittleFS.usedBytes();
        xSemaphoreGive(storageMutex);
    } else {
        totalBytes = 0;
        usedBytes = 0;
    }
}

//---- NVS (Preferences) Methods --------------------------------

void DataManager::saveDeviceID(String id) {
    prefs.begin(PREFS_NAMESPACE, false); // false = read/write
    prefs.putString(KEY_DEVICE_ID, id);
    prefs.end();
}

String DataManager::getDeviceID() {
    prefs.begin(PREFS_NAMESPACE, true); // true = read-only
    String id = prefs.getString(KEY_DEVICE_ID, "DEFAULT-000");
    prefs.end();
    return id;
}

void DataManager::saveWifiCredentials(String ssid, String password) {
    prefs.begin(PREFS_NAMESPACE, false);
    prefs.putString(KEY_WIFI_SSID, ssid);
    prefs.putString(KEY_WIFI_PASS, password);
    prefs.end();
    LOG_PRINTLN("WiFi credentials saved to NVS.");
}

String DataManager::getWifiSSID() {
    prefs.begin(PREFS_NAMESPACE, true);
    String ssid = prefs.getString(KEY_WIFI_SSID, "");
    prefs.end();
    return ssid;
}

String DataManager::getWifiPassword() {
    prefs.begin(PREFS_NAMESPACE, true);
    String pass = prefs.getString(KEY_WIFI_PASS, "");
    prefs.end();
    return pass;
}

//---- LittleFS Methods -----------------------------------------

void DataManager::saveDogList(String jsonString) {
    if (xSemaphoreTake(storageMutex, portMAX_DELAY) == pdTRUE) {
        File file = LittleFS.open(FILE_DOG_LIST, "w");
        if (file) {
            file.print(jsonString);
            file.close();
        } else {
            LOG_PRINTLN("Failed to open dog_list for writing.");
        }
        xSemaphoreGive(storageMutex);
    }
}

String DataManager::readDogList() {
    String content = "";
    if (xSemaphoreTake(storageMutex, portMAX_DELAY) == pdTRUE) {
        File file = LittleFS.open(FILE_DOG_LIST, "r");
        if (file) {
            content = file.readString();
            file.close();
        } else {
            LOG_PRINTLN("Dog list file not found.");
        }
        xSemaphoreGive(storageMutex);
    }
    return content;
}

bool DataManager::saveSessionFile(String sessionJsonString) {
    bool success = false;
    
    String path = String(DIR_SESSIONS) + "/" + String(millis()) + ".json";

    if (xSemaphoreTake(storageMutex, portMAX_DELAY) == pdTRUE) {
        LOG_PRINTF("DataManager: Got mutex, saving session to: %s\n", path.c_str());
        
        File file = LittleFS.open(path, "w"); // "w" = Write
        
        if (!file) {
            LOG_PRINTLN("Failed to create session file.");
        } else {
            if (file.print(sessionJsonString)) {
                success = true;
            }
            file.close();
        }
        xSemaphoreGive(storageMutex);
    }
    
    return success;
}

File DataManager::openSessionDirectory() {
    return LittleFS.open(DIR_SESSIONS, "r");
}

void DataManager::deleteSessionFile(String path) {
    // Esta función debe ser llamada por la SyncTask MIENTRAS
    // todavía tiene el Mutex.
    if (LittleFS.remove(path)) {
        LOG_PRINTF("Deleted session file: %s\n", path.c_str());
    } else {
        LOG_PRINTF("Error deleting session file: %s\n", path.c_str());
    }
}

bool DataManager::sessionFilesExist() {
    bool filesFound = false;
    if (xSemaphoreTake(storageMutex, portMAX_DELAY) == pdTRUE) {
        File root = LittleFS.open(DIR_SESSIONS, "r");
        if (root) {
            File file = root.openNextFile();
            if (file) {
                filesFound = true;
                file.close();
            }
            root.close();
        }
        xSemaphoreGive(storageMutex);
    }
    return filesFound;
}

SemaphoreHandle_t DataManager::getMutex() {
    return storageMutex;
}
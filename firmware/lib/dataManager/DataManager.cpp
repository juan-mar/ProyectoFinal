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

DataManager::DataManager() {
    
}

bool DataManager::init() {
    LOG_PRINTLN("Initializing DataManager...");
    
    if (!LittleFS.begin()) {
        LOG_PRINTLN("Failed to mount LittleFS! Formatting...");
        // If mounting fails, format it once
        if (LittleFS.format()) {
            LOG_PRINTLN("LittleFS formatted successfully.");
            return LittleFS.begin(); // Try mounting again
        } else {
            LOG_PRINTLN("FATAL: Failed to format LittleFS.");
            return false;
        }
    }
    LOG_PRINTLN("LittleFS mounted.");
    return true;
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
    File file = LittleFS.open(FILE_DOG_LIST, "w"); // "w" = Write (overwrite)
    if (!file) {
        LOG_PRINTLN("Failed to open dog_list for writing.");
        return;
    }
    file.print(jsonString);
    file.close();
}

String DataManager::readDogList() {
    File file = LittleFS.open(FILE_DOG_LIST, "r"); // "r" = Read
    if (!file) {
        LOG_PRINTLN("Dog list file not found.");
        return "";
    }
    String content = file.readString();
    file.close();
    return content;
}

bool DataManager::appendSessionLog(String jsonString) {
    // "a" = Append (adds to end of file, creates if not exists)
    File file = LittleFS.open(FILE_SESSIONS, "a");
    if (!file) {
        LOG_PRINTLN("Failed to open session log for appending.");
        return false;
    }
    
    bool success = file.println(jsonString); // Use println to separate entries
    file.close();
    return success;
}

File DataManager::openSessionLog() {
    // Returns the File object to be read by SyncState
    return LittleFS.open(FILE_SESSIONS, "r");
}

void DataManager::deleteSessionLog() {
    if (LittleFS.remove(FILE_SESSIONS)) {
        LOG_PRINTLN("Session log deleted.");
    } else {
        LOG_PRINTLN("Error deleting session log (may not exist).");
    }
}

bool DataManager::sessionLogExists() {
    if (LittleFS.exists(FILE_SESSIONS)) {
        File f = LittleFS.open(FILE_SESSIONS, "r");
        bool hasContent = f.size() > 0;
        f.close();
        return hasContent;
    }
    return false;
}
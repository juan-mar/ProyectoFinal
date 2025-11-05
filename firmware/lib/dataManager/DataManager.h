/****************************************************************
 * @file DataManager.h
 * @brief Declares the DataManager class, which handles all
 * non-volatile memory operations (NVS and LittleFS).
 ****************************************************************/

/****************************************************************
 * Include Guards
 ****************************************************************/
#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

/****************************************************************
 * Headers
 ****************************************************************/
#include <Arduino.h>
#include <Preferences.h> // For NVS
#include "FS.h"       // For File System base class
#include "LittleFS.h" // For LittleFS implementation
#include <freertos/semphr.h>

/****************************************************************
 * Class Declarations
 ****************************************************************/

/**
 * @brief Manages all data persistence (config and session logs).
 * This class abstracts away Preferences (NVS) and LittleFS.
 */
class DataManager {
public:
    /**
     * @brief Constructor
     */
    DataManager();

    /**
     * @brief Initializes the memory components (NVS and LittleFS).
     * Must be called once in setup().
     * @return true if all components initialized successfully.
     */
    bool init();

//---- NVS (Preferences) Methods --------------------------------

    /**
     * @brief Saves the device's unique ID to NVS.
     * @param id The ID to save (e.g., "ESP32-001").
     */
    void saveDeviceID(String id);

    /**
     * @brief Retrieves the device's unique ID from NVS.
     * @return The saved ID, or "DEFAULT-000" if not set.
     */
    String getDeviceID();

    /**
     * @brief Saves WiFi credentials to NVS.
     * @param ssid The WiFi SSID.
     * @param password The WiFi password.
     */
    void saveWifiCredentials(String ssid, String password);

    /**
     * @brief Gets the saved WiFi SSID.
     * @return The saved SSID, or an empty string if not set.
     */
    String getWifiSSID();

    /**
     * @brief Gets the saved WiFi password.
     * @return The saved password, or an empty string if not set.
     */
    String getWifiPassword();

    
//---- LittleFS Methods -----------------------------------------
    /**
     * @brief Saves the dog list (as a JSON string) to a file.
     * This will *overwrite* the existing file.
     * @param jsonString The JSON string to save.
     */
    void saveDogList(String jsonString);

    /**
     * @brief Reads the entire dog list file.
     * @return A String containing the dog list JSON, or empty if not found.
     */
    String readDogList();

    /**
     * @brief Appends a single session to the log file. Uses Mutex.
     * @param session The TrainingSession object to serialize and save.
     * @return true if append was successful.
     */
    bool saveSessionLog(String jsonString);

    /**
     * @brief Appends a single session (as a JSON string) to the log file.
     * @param jsonString The JSON string for the single session.
     * @return true if append was successful.
     */
    bool appendSessionLog(String jsonString);

    /**
     * @brief Opens the session log file for reading.
     * Used by SyncState to read sessions in batches.
     * @return A File object. The caller *must* close this file.
     */
    File openSessionLog();

    /**
     * @brief Deletes the session log file.
     * Called by SyncState after a successful batch upload.
     */
    void deleteSessionLog();

    /**
     * @brief Checks if a session log file exists.
     * @return true if the file exists and is not empty.
     */
    bool sessionLogExists();

private:
    Preferences prefs;
    SemaphoreHandle_t storageMutex;
    const char* PREFS_NAMESPACE = "config";
    const char* FILE_DOG_LIST = "/dog_list.json";
    const char* FILE_SESSIONS = "/sessions.log";
};

#endif // DATA_MANAGER_H
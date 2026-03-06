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
 * Enums and Constants
 ****************************************************************/

/**
 * @brief Result of session file validation.
 */
enum ValidationResult {
    VALID,                  // File is valid, ready to upload
    RECOVERABLE,            // File has invalid optional fields, can be cleaned
    UNRECOVERABLE           // File has invalid critical fields, must be discarded
};

// Session validation limits
#define MIN_SESSION_DURATION_S 5
#define MAX_SESSION_DURATION_S 3600
#define MAX_SESSION_TIMEOUT_S 7200
#define MIN_VALID_TEMPERATURE_C -20.0f
#define MAX_VALID_TEMPERATURE_C 50.0f
#define MIN_VALID_HUMIDITY_PCT 0.0f
#define MAX_VALID_HUMIDITY_PCT 100.0f
#define MIN_VALID_PRESSURE_HPA 900.0f
#define MAX_VALID_PRESSURE_HPA 1100.0f

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
     * @brief Destructor.
     * Cleans up system resources.
     */
    ~DataManager();
    
    /**
     * @brief Initializes peripherals (NVS, LittleFS).
     * This function is idempotent (safe to call multiple times).
     * @return true if all components initialized successfully.
     */
    bool init();

//---- State Management Methods ----------------------------

    /**
     * @brief Counts the number of session files currently pending.
     * @return The total number of files in the session directory.
     */
    int countPendingSessions();

    /**
     * @brief Gets the total and used bytes on the LittleFS partition.
     * @param totalBytes (out) A reference to store the total bytes.
     * @param usedBytes (out) A reference to store the used bytes.
     */
    void getStorageUsage(size_t &totalBytes, size_t &usedBytes);

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
     * @brief Appends a single session (as a JSON string) to the log file.
     * This is THREAD-SAFE (uses a Mutex).
     * @param sessionJsonString The complete JSON string for the session.
     * @param startedAt The ISO8601 timestamp to use as the filename (e.g., "2024-01-15T10:30:00.000Z").
     * @return true if append was successful.
     */
    bool saveSessionFile(String sessionJsonString, String startedAt);

    /**
     * @brief Opens the session directory ("/sessions") for reading.
     * Used by SyncState to iterate through session files.
     * @return A File object (directory). The caller *must* close this.
     */
    File openSessionDirectory();

    /**
     * @brief Deletes a single session file.
     * Called by SyncState after a file is successfully uploaded.
     * @param path The full path to the file (e.g., "/sessions/123456.json").
     */
    void deleteSessionFile(String path);

    /**
     * @brief Checks if any session files are pending upload.
     * @return true if the /sessions directory exists and is not empty.
     */
    bool sessionFilesExist();

    /**
     * @brief Prints all session files to Serial for debugging.
     * This will display the filename and JSON content of each saved training session.
     */
    void printAllSessionFiles();

    /**
     * @brief Validates a session file JSON and checks for critical fields.
     * @param jsonContent The JSON string of the session file.
     * @return VALID (ready to upload), RECOVERABLE (has invalid optional fields),
     *         or UNRECOVERABLE (has invalid critical fields).
     */
    ValidationResult validateSessionFile(const String &jsonContent);

    /**
     * @brief Cleans invalid optional fields from a session file and re-saves it.
     * Only call this if validateSessionFile() returned RECOVERABLE.
     * @param filePath The path to the session file to clean.
     * @return true if cleaned successfully.
     */
    bool cleanAndSaveSessionFile(String filePath);

    SemaphoreHandle_t getMutex();

private:
    Preferences prefs;
    SemaphoreHandle_t storageMutex;
    const char* PREFS_NAMESPACE = "config";
    const char* FILE_DOG_LIST = "/dog_list.json";
    const char* DIR_SESSIONS = "/sessions";

    /**
     * @brief Flag to make init() idempotent.
     */
    bool isInitialized;
    
};

#endif // DATA_MANAGER_H
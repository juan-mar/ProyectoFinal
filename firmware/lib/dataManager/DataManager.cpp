/****************************************************************
 * @file DataManager.cpp
 * @brief Implements the DataManager class methods.
 ****************************************************************/

/****************************************************************
 * Headers
 ****************************************************************/
#include "DataManager.h"
#include "config.h" // For logging
#include <ArduinoJson.h>

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
    id.trim();
    if (id.length() == 0) {
        return "DEFAULT-000";
    }
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

void DataManager::printAllSessionFiles() {
    LOG_PRINTLN("\n========== SESSION FILES ==========");
    
    if (xSemaphoreTake(storageMutex, portMAX_DELAY) == pdTRUE) {
        File root = LittleFS.open(DIR_SESSIONS, "r");
        if (!root) {
            LOG_PRINTLN("ERROR: Session directory not found.");
            xSemaphoreGive(storageMutex);
            return;
        }

        int fileCount = 0;
        File file = root.openNextFile();
        
        if (!file) {
            LOG_PRINTLN("No session files found.");
        } else {
            while (file) {
                if (!file.isDirectory()) {
                    fileCount++;
                    LOG_PRINTF("\n--- FILE %d ---\n", fileCount);
                    LOG_PRINTF("Name: %s\n", file.name());
                    LOG_PRINTF("Size: %d bytes\n", file.size());
                    
                    String content = file.readString();
                    LOG_PRINTLN("Content:");
                    LOG_PRINTLN(content);
                }
                file.close();
                file = root.openNextFile();
            }
        }
        
        root.close();
        LOG_PRINTF("\nTotal files: %d\n", fileCount);
        xSemaphoreGive(storageMutex);
    } else {
        LOG_PRINTLN("ERROR: Could not acquire mutex.");
    }
    
    LOG_PRINTLN("==================================\n");
}

ValidationResult DataManager::validateSessionFile(const String &jsonContent) {
    StaticJsonDocument<512> doc;
    
    // Intentar parsear el JSON
    DeserializationError error = deserializeJson(doc, jsonContent);
    if (error) {
        LOG_PRINTF("ERROR: Invalid JSON in session file: %s\n", error.c_str());
        return UNRECOVERABLE;
    }

    // Validar campos CRÍTICOS
    // Estos no pueden estar vacíos/nulos
    if (!doc.containsKey("p_dog_code") || doc["p_dog_code"].isNull()) {
        LOG_PRINTLN("[Validation] ERROR: p_dog_code is missing or null (CRITICAL)");
        return UNRECOVERABLE;
    }
    String dogCode = doc["p_dog_code"].as<String>();
    if (dogCode.length() == 0) {
        LOG_PRINTLN("[Validation] ERROR: p_dog_code is empty (CRITICAL)");
        return UNRECOVERABLE;
    }

    if (!doc.containsKey("p_started_at") || doc["p_started_at"].isNull()) {
        LOG_PRINTLN("[Validation] ERROR: p_started_at is missing or null (CRITICAL)");
        return UNRECOVERABLE;
    }
    String startedAt = doc["p_started_at"].as<String>();
    if (startedAt.length() == 0) {
        LOG_PRINTLN("[Validation] ERROR: p_started_at is empty (CRITICAL)");
        return UNRECOVERABLE;
    }

    if (!doc.containsKey("p_result") || doc["p_result"].isNull()) {
        LOG_PRINTLN("[Validation] ERROR: p_result is missing or null (CRITICAL)");
        return UNRECOVERABLE;
    }
    String result = doc["p_result"].as<String>();
    if (result.length() == 0) {
        LOG_PRINTLN("[Validation] ERROR: p_result is empty (CRITICAL)");
        return UNRECOVERABLE;
    }

    if (!doc.containsKey("p_device_code") || doc["p_device_code"].isNull()) {
        LOG_PRINTLN("[Validation] ERROR: p_device_code is missing or null (CRITICAL)");
        return UNRECOVERABLE;
    }
    String deviceCode = doc["p_device_code"].as<String>();
    if (deviceCode.length() == 0) {
        LOG_PRINTLN("[Validation] ERROR: p_device_code is empty (CRITICAL)");
        return UNRECOVERABLE;
    }

    // Validar campos OPCIONALES (solo para detectar si están inválidos)
    bool hasIssuesInOptional = false;

    // Validar p_duration_s (debe estar en rango válido si existe)
    if (doc.containsKey("p_duration_s")) {
        if (doc["p_duration_s"].is<int>()) {
            int duration = doc["p_duration_s"].as<int>();
            if (duration < MIN_SESSION_DURATION_S || duration > MAX_SESSION_DURATION_S) {
                LOG_PRINTF("[Validation] WARNING: p_duration_s out of range (%d s, valid: %d-%d)\n", 
                           duration, MIN_SESSION_DURATION_S, MAX_SESSION_DURATION_S);
                hasIssuesInOptional = true;
            }
        }
    }

    // Validar p_timeout_s (debe estar en rango válido si existe)
    if (doc.containsKey("p_timeout_s")) {
        if (doc["p_timeout_s"].is<int>()) {
            int timeout = doc["p_timeout_s"].as<int>();
            if (timeout < 0 || timeout > MAX_SESSION_TIMEOUT_S) {
                LOG_PRINTF("[Validation] WARNING: p_timeout_s out of range (%d s, max: %d)\n", 
                           timeout, MAX_SESSION_TIMEOUT_S);
                hasIssuesInOptional = true;
            }
        }
    }

    // Validar p_conditions (debe ser un objeto válido si existe, y chequear temperatura/humedad/presion)
    if (doc.containsKey("p_conditions")) {
        if (!doc["p_conditions"].is<JsonObject>()) {
            LOG_PRINTLN("[Validation] WARNING: p_conditions is not a valid JSON object");
            hasIssuesInOptional = true;
        } else {
            JsonObject conditions = doc["p_conditions"];
            
            // Validar temperatura si existe
            if (conditions.containsKey("temp")) {
                float temp = conditions["temp"].as<float>();
                if (temp < MIN_VALID_TEMPERATURE_C || temp > MAX_VALID_TEMPERATURE_C) {
                    LOG_PRINTF("[Validation] WARNING: temperature out of range (%.1f°C, valid: %.1f-%.1f)\n",
                               temp, MIN_VALID_TEMPERATURE_C, MAX_VALID_TEMPERATURE_C);
                    hasIssuesInOptional = true;
                }
            }
            
            // Validar humedad si existe
            if (conditions.containsKey("humidity")) {
                float humidity = conditions["humidity"].as<float>();
                if (humidity < MIN_VALID_HUMIDITY_PCT || humidity > MAX_VALID_HUMIDITY_PCT) {
                    LOG_PRINTF("[Validation] WARNING: humidity out of range (%.1f%%, valid: %.1f-%.1f)\n",
                               humidity, MIN_VALID_HUMIDITY_PCT, MAX_VALID_HUMIDITY_PCT);
                    hasIssuesInOptional = true;
                }
            }
            
            // Validar presión si existe
            if (conditions.containsKey("pressure")) {
                float pressure = conditions["pressure"].as<float>();
                if (pressure < MIN_VALID_PRESSURE_HPA || pressure > MAX_VALID_PRESSURE_HPA) {
                    LOG_PRINTF("[Validation] WARNING: pressure out of range (%.1f hPa, valid: %.1f-%.1f)\n",
                               pressure, MIN_VALID_PRESSURE_HPA, MAX_VALID_PRESSURE_HPA);
                    hasIssuesInOptional = true;
                }
            }
        }
    }

    // Validar p_type (debe ser un objeto válido si existe)
    if (doc.containsKey("p_type")) {
        if (!doc["p_type"].is<JsonObject>()) {
            LOG_PRINTLN("[Validation] WARNING: p_type is not a valid JSON object");
            hasIssuesInOptional = true;
        }
    }

    // Determinar resultado
    if (hasIssuesInOptional) {
        LOG_PRINTLN("[Validation] File is RECOVERABLE (has invalid optional fields)");
        return RECOVERABLE;
    } else {
        LOG_PRINTLN("[Validation] File is VALID");
        return VALID;
    }
}

bool DataManager::cleanAndSaveSessionFile(String filePath) {
    bool success = false;
    
    if (xSemaphoreTake(storageMutex, portMAX_DELAY) == pdTRUE) {
        File file = LittleFS.open(filePath, "r");
        if (!file) {
            LOG_PRINTF("ERROR: Cannot open file to clean: %s\n", filePath.c_str());
            xSemaphoreGive(storageMutex);
            return false;
        }

        String jsonContent = file.readString();
        file.close();

        // Parsear el JSON
        DynamicJsonDocument doc(512);
        if (deserializeJson(doc, jsonContent)) {
            LOG_PRINTLN("ERROR: Cannot parse JSON while cleaning");
            xSemaphoreGive(storageMutex);
            return false;
        }

        // Limpiar campos opcionales inválidos
        bool modified = false;

        // Limpiar p_duration_s si está fuera de rango
        if (doc.containsKey("p_duration_s") && doc["p_duration_s"].is<int>()) {
            int duration = doc["p_duration_s"].as<int>();
            if (duration < MIN_SESSION_DURATION_S || duration > MAX_SESSION_DURATION_S) {
                LOG_PRINTF("[Clean] Removing invalid p_duration_s: %d s\n", duration);
                doc.remove("p_duration_s");
                modified = true;
            }
        }

        // Limpiar p_timeout_s si está fuera de rango
        if (doc.containsKey("p_timeout_s") && doc["p_timeout_s"].is<int>()) {
            int timeout = doc["p_timeout_s"].as<int>();
            if (timeout < 0 || timeout > MAX_SESSION_TIMEOUT_S) {
                LOG_PRINTF("[Clean] Removing invalid p_timeout_s: %d s\n", timeout);
                doc.remove("p_timeout_s");
                modified = true;
            }
        }

        // Limpiar p_conditions si no es un objeto válido o tiene valores fuera de rango
        if (doc.containsKey("p_conditions")) {
            if (!doc["p_conditions"].is<JsonObject>()) {
                LOG_PRINTLN("[Clean] Removing invalid p_conditions (not an object)");
                doc.remove("p_conditions");
                modified = true;
            } else {
                JsonObject conditions = doc["p_conditions"];
                bool conditionsInvalid = false;
                
                // Verificar temperatura
                if (conditions.containsKey("temp")) {
                    float temp = conditions["temp"].as<float>();
                    if (temp < MIN_VALID_TEMPERATURE_C || temp > MAX_VALID_TEMPERATURE_C) {
                        LOG_PRINTF("[Clean] Temperature out of range: %.1f°C\n", temp);
                        conditionsInvalid = true;
                    }
                }
                
                // Verificar humedad
                if (conditions.containsKey("humidity")) {
                    float humidity = conditions["humidity"].as<float>();
                    if (humidity < MIN_VALID_HUMIDITY_PCT || humidity > MAX_VALID_HUMIDITY_PCT) {
                        LOG_PRINTF("[Clean] Humidity out of range: %.1f%%\n", humidity);
                        conditionsInvalid = true;
                    }
                }
                
                // Verificar presión
                if (conditions.containsKey("pressure")) {
                    float pressure = conditions["pressure"].as<float>();
                    if (pressure < MIN_VALID_PRESSURE_HPA || pressure > MAX_VALID_PRESSURE_HPA) {
                        LOG_PRINTF("[Clean] Pressure out of range: %.1f hPa\n", pressure);
                        conditionsInvalid = true;
                    }
                }
                
                if (conditionsInvalid) {
                    LOG_PRINTLN("[Clean] Removing invalid p_conditions");
                    doc.remove("p_conditions");
                    modified = true;
                }
            }
        }

        // Limpiar p_type si no es un objeto válido
        if (doc.containsKey("p_type")) {
            if (!doc["p_type"].is<JsonObject>()) {
                LOG_PRINTLN("[Clean] Removing invalid p_type");
                doc.remove("p_type");
                modified = true;
            }
        }

        if (modified) {
            // Serializar nuevamente
            String cleanedJson;
            serializeJson(doc, cleanedJson);

            // Sobrescribir el archivo con el contenido limpio
            File outFile = LittleFS.open(filePath, "w");
            if (outFile) {
                outFile.print(cleanedJson);
                outFile.close();
                LOG_PRINTF("[Clean] File cleaned and saved: %s\n", filePath.c_str());
                success = true;
            } else {
                LOG_PRINTF("ERROR: Cannot open file for writing: %s\n", filePath.c_str());
            }
        } else {
            LOG_PRINTLN("[Clean] File was already clean");
            success = true;
        }

        xSemaphoreGive(storageMutex);
    }
    
    return success;
}

SemaphoreHandle_t DataManager::getMutex() {
    return storageMutex;
}
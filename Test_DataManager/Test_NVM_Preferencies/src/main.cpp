/****************************************************************
 * @file main.cpp
 * @brief NVS (Preferences)
 * Testbench for writing, reading, and clearing key-value
 * pairs from Non-Volatile Storage.
 ****************************************************************/

/****************************************************************
 * Headers
 ****************************************************************/
#include <Arduino.h>
#include <Preferences.h> // The NVS driver

/****************************************************************
 * Globals
 ****************************************************************/
/**
 * @brief Create an instance of the Preferences library.
 */
Preferences prefs;

/**
 * @brief Define the "namespace" for our app's settings.
 * This acts like a separate "folder" inside the NVS.
 */
const char* PREFS_NAMESPACE = "device_cfg";

/**
 * @brief Define the keys we will use.
 * ¡¡IMPORTANTE: Las claves tienen un límite de 15 caracteres!!
 */
const char* KEY_DEVICE_ID = "dev_id";
const char* KEY_WIFI_SSID = "wifi_ssid";
const char* KEY_AUTO_MODE = "auto_mode"; 

/****************************************************************
 * Helper Function Prototypes
 ****************************************************************/

/**
 * @brief (Test 'w') Writes hardcoded values to NVS.
 */
void writeTestConfig();

/**
 * @brief (Test 'r') Reads all known values from NVS.
 */
void readConfig();

/**
 * @brief (Test 'c') Clears all keys in our namespace.
 */
void clearConfig();

/**
 * @brief (Test 'b') Writes a boolean value to NVS.
 */
void writeBoolValue();

/**
 * @brief (Test 'k') Removes a single key (WIFI_SSID).
 */
void removeOneKey();

/****************************************************************
 * Setup Function
 ****************************************************************/
void setup() {
    Serial.begin(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.println("\n--- NVS (Preferences) Sandbox ---");
    Serial.println("Send commands via Serial Monitor (No new line/CR):");
    Serial.println(" 'w' -> Write/Save test configuration");
    Serial.println(" 'r' -> Read current configuration");
    Serial.println(" 'c' -> Clear configuration");

    Serial.println("\nReading initial config on boot:");
    readConfig();
}

/****************************************************************
 * Loop Function (Test Trigger)
 ****************************************************************/
void loop() {
    if (Serial.available() > 0) {
        char command = Serial.read();

       if (command == 'w') {
            Serial.println("\n[Test 'w'] Writing test config (string)...");
            writeTestConfig();
            Serial.println("Done. Reading config back:");
            readConfig();
        
        } else if (command == 'r') {
            Serial.println("\n[Test 'r'] Reading current config...");
            readConfig();

        } else if (command == 'c') {
            Serial.println("\n[Test 'c'] Clearing *all* config...");
            clearConfig();
            Serial.println("Done. Reading config back (should be defaults):");
            readConfig();
        
        } else if (command == 'b') {
            Serial.println("\n[Test 'b'] Writing boolean (auto_mode = true)...");
            writeBoolValue();
            Serial.println("Done. Reading config back:");
            readConfig();

        } else if (command == 'k') {
            Serial.println("\n[Test 'k'] Removing only wifi_ssid key...");
            removeOneKey();
            Serial.println("Done. Reading config back:");
            readConfig();
        }
    }
    
    vTaskDelay(100 / portTICK_PERIOD_MS);
}

/****************************************************************
 * Helper Function Implementations
 ****************************************************************/

void writeTestConfig() {
    // 1. Abre el namespace en modo lectura-escritura
    if (!prefs.begin(PREFS_NAMESPACE, false)) { // false = read/write, true = read-only
        Serial.println("Error opening preferences!");
        return;
    }

    // 2. Escribe los valores
    // (Devuelven el nro de bytes escritos, > 0 es éxito)
    prefs.putString(KEY_DEVICE_ID, "ESP32-001");
    prefs.putString(KEY_WIFI_SSID, "MiRedDePrueba");
    
    // 3. Cierra (guarda) el namespace
    prefs.end();
}

void readConfig() {
    // 1. Abre el namespace en modo solo-lectura
    if (!prefs.begin(PREFS_NAMESPACE, true)) { // true = read-only
        Serial.println("Error opening preferences in read-only mode.");
        return;
    }

    // 2. Lee los valores usando un VALOR POR DEFECTO.
    // Si la clave no existe, getString() devuelve el segundo parámetro.
    String deviceId = prefs.getString(KEY_DEVICE_ID, "DEFAULT_ID");
    String wifiSsid = prefs.getString(KEY_WIFI_SSID, "NO_SSID_SET");

    // Lee el nuevo valor booleano (default es 'false')
    bool autoMode = prefs.getBool(KEY_AUTO_MODE, false);
    
    // 3. Cierra el namespace
    prefs.end();

    // 4. Imprime los resultados
    Serial.println("--- Current Config ---");
    Serial.printf("Device ID:  %s\n", deviceId.c_str());
    Serial.printf("WiFi SSID:  %s\n", wifiSsid.c_str());
    Serial.printf("Auto Mode:  %s\n", autoMode ? "true" : "false");
    Serial.println("------------------------");
}

void clearConfig() {
    if (!prefs.begin(PREFS_NAMESPACE, false)) {
        Serial.println("Error opening preferences!");
        return;
    }

    // Borra TODAS las claves dentro del namespace "device_cfg"
    prefs.clear();
    
    prefs.end();
}

void writeBoolValue() {
    if (!prefs.begin(PREFS_NAMESPACE, false)) {
        Serial.println("Error opening preferences!");
        return;
    }
    // Escribe un booleano
    prefs.putBool(KEY_AUTO_MODE, true);
    prefs.end();
}

void removeOneKey() {
    if (!prefs.begin(PREFS_NAMESPACE, false)) {
        Serial.println("Error opening preferences!");
        return;
    }
    // Borra SOLO la clave especificada
    prefs.remove(KEY_WIFI_SSID);
    prefs.end();
}
/****************************************************************
 * @file main.cpp
 * @brief Full Integration Sandbox (DataManager + SupabaseClient)
 * Testbench for the complete offline->online sync workflow.
 ****************************************************************/

/****************************************************************
 * Headers
 ****************************************************************/
#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "credentials.h"
#include "DataManager.h"
#include "TrainingSession.h"
#include "SupabaseClient.h"

/****************************************************************
 * Defines
 ****************************************************************/
#define BATCH_SIZE 5 // ¿Cuántas sesiones subir por lote?

/****************************************************************
 * Global Variables
 ****************************************************************/
DataManager* g_dataManager = nullptr;
SupabaseClient* g_supabaseClient = nullptr;
String g_accessToken = ""; // Para guardar el token de login

/****************************************************************
 * Helper Function Prototypes
 ****************************************************************/
void connectWiFi();
void testWriteSession();
void testReadSessions();
void testSyncSessions(); // ¡LA FUNCIÓN CLAVE!
void testShowStatus();

/****************************************************************
 * Setup Function
 ****************************************************************/
void setup() {
    LOG_SETUP(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS); 
    LOG_PRINTLN("\n--- Full Integration Sandbox ---");

    // 1. Crear e inicializar DataManager
    g_dataManager = new DataManager();
    if (!g_dataManager->init()) {
        LOG_PRINTLN("FATAL: Failed to initialize DataManager!");
        while(1);
    }
    // Guardar un ID de dispositivo para la prueba
    g_dataManager->saveDeviceID("ESP32-001");

    // 2. Crear SupabaseClient
    g_supabaseClient = new SupabaseClient(SUPABASE_URL, SUPABASE_API_KEY);

    // 3. Conectar al WiFi
    connectWiFi();

    LOG_PRINTLN("\n--- Event Simulator Ready ---");
    LOG_PRINTLN("Send commands via Serial Monitor (No new line/CR):");
    LOG_PRINTLN(" 's' -> Show Status (count files)");
    LOG_PRINTLN(" 'w' -> Write a new test session file");
    LOG_PRINTLN(" 'r' -> Read all pending session files");
    LOG_PRINTLN(" 'd' -> (No implementado) Borrar manualmente");
    LOG_PRINTLN(" 'S' -> (Mayúscula) SYNC pending sessions to Supabase");

    testShowStatus(); // Muestra el estado inicial
}

/****************************************************************
 * Loop Function (Test Trigger)
 ****************************************************************/
void loop() {
#if DEBUG_MODE == 1
    if (Serial.available() > 0) {
        char command = Serial.read();

        switch (command) {
            case 's':
                LOG_PRINTLN("\n[Test 's'] Getting storage status...");
                testShowStatus();
                break;
            case 'w':
                LOG_PRINTLN("\n[Test 'w'] Simulating new training session...");
                testWriteSession();
                testShowStatus();
                break;
            case 'r':
                LOG_PRINTLN("\n[Test 'r'] Reading all pending session files...");
                testReadSessions();
                break;
            case 'S': // 'S' Mayúscula para la sincronización
                LOG_PRINTLN("\n[Test 'S'] === STARTING SYNC PROCESS ===");
                testSyncSessions();
                testShowStatus();
                break;
        }
    }
#endif
    vTaskDelay(50 / portTICK_PERIOD_MS);
}

/****************************************************************
 * WiFi Connect
 ****************************************************************/
void connectWiFi() {
    LOG_PRINTF("Connecting to WiFi: %s\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
        LOG_PRINT(".");
    }
    LOG_PRINTLN("\nWiFi Connected!");
}

/****************************************************************
 * Test Function Implementations
 ****************************************************************/

void testWriteSession() {
    // 1. Crear y serializar la sesión
    TrainingSession session;
    session.setDeviceCode(g_dataManager->getDeviceID());
    session.setDogCode("SIMON-01");
    String timestamp = "2025-11-16T" + String(random(10,19)) + ":" + String(random(10,59)) + ":00Z";
    session.setStartedAt(timestamp);
    session.setDuration(random(30, 180));
    session.setResult("success");
    session.setConditions("{\"temp\":25.0}");
    session.setType("{\"scent\":\"Narcoticos\", \"mode\":\"auto\"}");

    String jsonString;
    if (!session.serialize(jsonString)) {
        LOG_PRINTLN("ERROR: Failed to serialize session!");
        return;
    }

    LOG_PRINTLN("--- Serialized JSON to be saved ---");
    LOG_PRINTLN(jsonString);
    
    // 2. Guardar en el DataManager
    if (g_dataManager->saveSessionFile(jsonString)) {
        LOG_PRINTLN("SUCCESS: Session file saved to LittleFS.");
    } else {
        LOG_PRINTLN("ERROR: Failed to save session file!");
    }
}

void testReadSessions() {
    // Simula la SyncTask tomando el Mutex
    if (xSemaphoreTake(g_dataManager->getMutex(), portMAX_DELAY) == pdTRUE) {
        LOG_PRINTLN("ReadTest: Got mutex. Opening directory...");
        File root = g_dataManager->openSessionDirectory(); // Llama a la función "tonta"
        if (!root) {
            LOG_PRINTLN("ReadTest: Failed to open session directory.");
            xSemaphoreGive(g_dataManager->getMutex());
            return;
        }

        File file = root.openNextFile();
        if (!file) {
            LOG_PRINTLN("ReadTest: No session files found.");
        }

        while (file) {
            if (!file.isDirectory()) {
                LOG_PRINTF("--- Reading file: %s (%d bytes) ---\n", file.path(), file.size());
                while(file.available()){ Serial.write(file.read()); }
                Serial.println("\n-------------------------------------");
            }
            file.close();
            file = root.openNextFile();
        }
        
        root.close();
        LOG_PRINTLN("ReadTest: Finished reading. Releasing mutex.");
        xSemaphoreGive(g_dataManager->getMutex());
    }
}

void testShowStatus() {
    int count = g_dataManager->countPendingSessions();
    size_t totalBytes = 0, usedBytes = 0;
    g_dataManager->getStorageUsage(totalBytes, usedBytes);
    float usedPercent = (totalBytes > 0) ? (float)usedBytes * 100.0 / (float)totalBytes : 0;

    LOG_PRINTLN("--- Storage Status ---");
    LOG_PRINTF("Pending Sessions: %d\n", count);
    LOG_PRINTF("Storage Used: %.2f%% (%d / %d bytes)\n", usedPercent, usedBytes, totalBytes);
    LOG_PRINTLN("------------------------");
}

/**
 * @brief ¡ESTA ES LA LÓGICA DE SINCRONIZACIÓN COMPLETA!
 */
void testSyncSessions() {
    // 1. Login para obtener token fresco
    LOG_PRINTLN("SyncTest: Logging in...");
    if (!g_supabaseClient->login(DEVICE_EMAIL, DEVICE_PASSWORD, g_accessToken)) {
        LOG_PRINTLN("SyncTest: FATAL: Login failed. Aborting sync.");
        return;
    }
    LOG_PRINTLN("SyncTest: Login successful.");
    bool syncFailed = false;

    // 2. Tomar el Mutex (¡una sola vez!)
    if (xSemaphoreTake(g_dataManager->getMutex(), portMAX_DELAY) == pdTRUE) {
        LOG_PRINTLN("SyncTest: Got mutex. Opening session directory...");
        File root = g_dataManager->openSessionDirectory();
        if (!root) {
            LOG_PRINTLN("SyncTest: Failed to open session directory.");
            xSemaphoreGive(g_dataManager->getMutex());
            return;
        }

        File file = root.openNextFile();

        // 3. Bucle principal: se ejecuta mientras haya archivos
        while (file && !syncFailed) {
            
            // --- INICIO DEL LOTE ---
            LOG_PRINTLN("SyncTest: Starting new batch...");
            DynamicJsonDocument batchDoc(2048); // Búfer en HEAP
            JsonArray p_items = batchDoc.createNestedArray("p_items");
            
            String filePathsToDelete[BATCH_SIZE];
            int batchCount = 0;

            // 4. Bucle del Lote: Llenar el lote
            for (int i = 0; i < BATCH_SIZE && file; i++) {
                String path = file.path();
                String sessionJson = file.readString();
                file.close();

                DynamicJsonDocument tempDoc(512);
                if (deserializeJson(tempDoc, sessionJson) == DeserializationError::Ok) {
                    p_items.add(tempDoc.as<JsonObject>());
                    filePathsToDelete[batchCount] = path;
                    batchCount++;
                } else {
                    LOG_PRINTF("SyncTest: ERROR: Failed to parse JSON, deleting corrupt file: %s\n", path.c_str());
                    g_dataManager->deleteSessionFile(path); // Borra el archivo corrupto
                }
                
                file = root.openNextFile(); // Avanza al siguiente
            }

            // 5. Si el lote tiene items, subirlo
            if (batchCount > 0) {
                String batchJsonString;
                serializeJson(batchDoc, batchJsonString);
                
                LOG_PRINTF("SyncTest: Uploading batch of %d sessions...\n", batchCount);
                if (g_supabaseClient->recordTrainingBatch(g_accessToken, batchJsonString)) {
                    // 6. Éxito: Borrar los archivos subidos
                    LOG_PRINTLN("SyncTest: Batch upload successful. Deleting files...");
                    for (int i = 0; i < batchCount; i++) {
                        g_dataManager->deleteSessionFile(filePathsToDelete[i]);
                    }
                } else {
                    // 7. Fallo: Detener la sincronización
                    LOG_PRINTLN("SyncTest: Batch upload FAILED. Stopping sync process.");
                    syncFailed = true; // Detiene el bucle 'while(file)'
                }
            }
        } // Fin del bucle while(file)

        // 8. Limpieza final
        root.close();
        LOG_PRINTLN("SyncTest: Finished batch sync. Releasing mutex.");
        xSemaphoreGive(g_dataManager->getMutex());
    
    } else {
        LOG_PRINTLN("SyncTest: Could not get mutex.");
    }

    if (syncFailed) {
        LOG_PRINTLN("SyncTest: Sync process finished with errors.");
    } else {
        LOG_PRINTLN("SyncTest: Sync process completed successfully.");
    }
}
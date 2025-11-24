/****************************************************************
 * @file main.cpp
 * @brief DataManager & TrainingSession Sandbox
 * Testbench for the complete offline session storage workflow.
 ****************************************************************/

/****************************************************************
 * Headers
 ****************************************************************/
#include <Arduino.h>
#include "config.h"          // For LOG_... macros
#include "DataManager.h"       // The class we are testing
#include "TrainingSession.h" // The data object

/****************************************************************
 * Global Variables
 ****************************************************************/
DataManager* g_dataManager = nullptr;
int sessionCounter = 0; // Para hacer los datos de prueba únicos

/****************************************************************
 * Helper Function Prototypes
 ****************************************************************/
void testWriteSession();
void testReadSessions();
void testDeleteSessions();
void testShowStatus();

/****************************************************************
 * Setup Function
 ****************************************************************/
void setup() {
    LOG_SETUP(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS); 
    LOG_PRINTLN("\n--- DataManager Sandbox ---");

    // 1. Crear e inicializar el DataManager
    g_dataManager = new DataManager();
    if (!g_dataManager->init()) {
        LOG_PRINTLN("FATAL: Failed to initialize DataManager!");
        while(1);
    }
    
    // Guardar un ID de dispositivo para la prueba
    g_dataManager->saveDeviceID("ESP32-TEST-001");

    LOG_PRINTLN("\n--- Event Simulator Ready ---");
    LOG_PRINTLN("Send commands via Serial Monitor (No new line/CR):");
    LOG_PRINTLN(" 's' -> Show Status (count files, storage usage)");
    LOG_PRINTLN(" 'w' -> Write a new test session file");
    LOG_PRINTLN(" 'r' -> Read all pending session files");
    LOG_PRINTLN(" 'd' -> Delete all pending session files");

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
                testShowStatus(); // Muestra el estado después de escribir
                break;
            case 'r':
                LOG_PRINTLN("\n[Test 'r'] Reading all pending session files...");
                testReadSessions();
                break;
            case 'd':
                LOG_PRINTLN("\n[Test 'd'] Deleting all pending session files...");
                testDeleteSessions();
                testShowStatus(); // Muestra el estado después de borrar
                break;
        }
    }
#endif

    vTaskDelay(50 / portTICK_PERIOD_MS);
}

/****************************************************************
 * Test Function Implementations
 ****************************************************************/

/**
 * @brief Simula la FSM creando y guardando una sesión.
 */
void testWriteSession() {
    // 1. Crear el objeto de datos (como lo haría ConfigState/PlayState)
    TrainingSession session;
    session.setDeviceCode(g_dataManager->getDeviceID());
    session.setDogCode("LUNA-002");
    
    // Simula un timestamp (en un caso real, vendría del celular)
    String timestamp = "2025-11-15T" + String(random(10,19)) + ":00:00Z";
    session.setStartedAt(timestamp);
    
    session.setDuration(random(30, 180));
    session.setResult((sessionCounter % 2 == 0) ? "success" : "fail");
    
    // Simula los JSON anidados
    session.setConditions("{\"temp\":22.5, \"wind\":\"N\"}");
    session.setType("{\"scent\":\"Explosivos\", \"mode\":\"auto\"}");

    // 2. Serializar el objeto (como lo haría el PlayState)
    String jsonString;
    if (!session.serialize(jsonString)) {
        LOG_PRINTLN("ERROR: Failed to serialize session!");
        return;
    }

    LOG_PRINTLN("--- Serialized JSON to be saved ---");
    LOG_PRINTLN(jsonString); // <-- ¡Verifica que este JSON coincida con la RPC!
    LOG_PRINTLN("-----------------------------------");

    // 3. Guardar en el DataManager
    if (g_dataManager->saveSessionFile(jsonString)) {
        LOG_PRINTLN("SUCCESS: Session file saved to LittleFS.");
        sessionCounter++;
    } else {
        LOG_PRINTLN("ERROR: Failed to save session file!");
    }
}

/**
 * @brief Simula la SyncTask leyendo todos los archivos.
 */
void testReadSessions() {
    // 1. Tomar el Mutex (¡Buena práctica!)
    if (xSemaphoreTake(g_dataManager->getMutex(), portMAX_DELAY) == pdTRUE) {
        
        LOG_PRINTLN("ReadTest: Got mutex. Opening directory...");
        File root = g_dataManager->openSessionDirectory();
        if (!root) {
            LOG_PRINTLN("ReadTest: Failed to open session directory.");
            xSemaphoreGive(g_dataManager->getMutex());
            return;
        }

        File file = root.openNextFile();
        if (!file) {
            LOG_PRINTLN("ReadTest: No session files found.");
        }

        // 2. Iterar sobre todos los archivos
        while (file) {
            if (!file.isDirectory()) {
                LOG_PRINTF("--- Reading file: %s (%d bytes) ---\n", file.path(), file.size());
                // Imprime el contenido del archivo
                while(file.available()){
                    Serial.write(file.read());
                }
                Serial.println("\n-------------------------------------");
            }
            file.close();
            file = root.openNextFile();
        }
        
        root.close();
        LOG_PRINTLN("ReadTest: Finished reading. Releasing mutex.");
        xSemaphoreGive(g_dataManager->getMutex());
    
    } else {
        LOG_PRINTLN("ReadTest: Could not get mutex.");
    }
}

/**
 * @brief Simula la SyncTask borrando archivos después de una subida.
 */
void testDeleteSessions() {
    if (xSemaphoreTake(g_dataManager->getMutex(), portMAX_DELAY) == pdTRUE) {
        LOG_PRINTLN("DeleteTest: Got mutex. Opening directory...");
        File root = g_dataManager->openSessionDirectory();
        if (!root) {
            LOG_PRINTLN("DeleteTest: Failed to open session directory.");
            xSemaphoreGive(g_dataManager->getMutex());
            return;
        }

        File file = root.openNextFile();
        while (file) {
            if (!file.isDirectory()) {
                String path = file.path();
                file.close(); // Cierra el archivo ANTES de borrarlo
                g_dataManager->deleteSessionFile(path); // Usa la función del DM
            }
            file = root.openNextFile();
        }
        
        root.close();
        LOG_PRINTLN("DeleteTest: Finished deleting. Releasing mutex.");
        xSemaphoreGive(g_dataManager->getMutex());
    
    } else {
        LOG_PRINTLN("DeleteTest: Could not get mutex.");
    }
}

/**
 * @brief Muestra el estado actual del almacenamiento.
 */
void testShowStatus() {
    int count = g_dataManager->countPendingSessions();
    
    size_t totalBytes = 0;
    size_t usedBytes = 0;
    g_dataManager->getStorageUsage(totalBytes, usedBytes);
    float usedPercent = (totalBytes > 0) ? (float)usedBytes * 100.0 / (float)totalBytes : 0;

    LOG_PRINTLN("--- Storage Status ---");
    LOG_PRINTF("Pending Sessions: %d\n", count);
    LOG_PRINTF("Storage Used: %.2f%% (%d / %d bytes)\n", usedPercent, usedBytes, totalBytes);
    LOG_PRINTLN("------------------------");
}
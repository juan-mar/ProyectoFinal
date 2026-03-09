/****************************************************************
 * @file SyncState.cpp
 * @brief Implements the SyncState class and its background sync task.
 ****************************************************************/

/****************************************************************
 * Headers
 ****************************************************************/
#include <Arduino.h>
#include "SyncState.h"
#include "StateManager.h"
#include "DataManager.h"
#include "SupabaseClient.h"
#include "HardwareManager.h"
#include "EventLogger.h"

#include "Events.h"
#include "config.h"
#include "credentials.h"

#include <WiFi.h>
#include <ArduinoJson.h>

// States we can transition to
#include "PowerUpState.h"
#include "IdleState.h"
#include "PowerOffState.h"

/****************************************************************
 * Defines and Constants
 ****************************************************************/
#define SYNC_TASK_STACK_SIZE    8192 // 8KB (JSON y HTTP necesitan mucho stack)
#define SYNC_TASK_PRIORITY      2      // Prioridad más alta que la FSM
#define SYNC_BATCH_SIZE         5         // Subir 5 sesiones a la vez

/****************************************************************
 * Task Parameters and Cancellation Flag
 ****************************************************************/

/**
 * @brief Parameters passed to the background sync task.
 */
struct SyncTaskParams {
    DataManager* dataManager;
    SupabaseClient* supabaseClient;
    HardwareManager* hardwareManager;
    QueueHandle_t fsmQueue;
};

/**
 * @brief Global flag to signal the task to stop.
 * 'volatile' ensures the compiler reads it fresh every time.
 */
static volatile bool g_cancelSync = false;

/**
 * @brief Global flag to track if SyncTask is running.
 * Used by exit() to wait for task completion before state change.
 */
static volatile bool g_syncTaskRunning = false;

/****************************************************************
 * Background Sync Task (La Lógica "Real")
 ****************************************************************/

/**
 * @brief The actual background task function that does the work.
 */
void syncTaskFunction(void* parameter) {
    LOG_PRINTLN("[SyncTask] Task started.");
    g_syncTaskRunning = true;  // Mark task as running
    
    // 1. Obtener parámetros
    SyncTaskParams* params = (SyncTaskParams*)parameter;
    DataManager* dataManager = params->dataManager;
    SupabaseClient* supabaseClient = params->supabaseClient;
    QueueHandle_t fsmQueue = params->fsmQueue;
    HardwareManager* hw = params->hardwareManager;
    
    //Send CMD
    //hw->setLedPattern(LED_SYNCING);

    String accessToken = "";
    bool syncFailed = false;

    // 2. Conectar al WiFi
    String ssid = dataManager->getWifiSSID();
    String pass = dataManager->getWifiPassword();
    if (ssid.length() == 0) {
        LOG_PRINTLN("[SyncTask] FATAL: No WiFi credentials set in NVS.");
        
        //Send CMD
        //ui->setLedPattern(LED_ERROR_WIFI);
        syncFailed = true;
    } else {
        LOG_PRINTF("[SyncTask] Connecting to WiFi: %s\n", ssid.c_str());
        // SOLUCIÓN 2: Reset WiFi mode explicitly before connecting
        WiFi.mode(WIFI_STA);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        WiFi.begin(ssid.c_str(), pass.c_str());
        
        int retries = 0;
        while (WiFi.status() != WL_CONNECTED && retries < 20) {
            if (g_cancelSync) {
                LOG_PRINTLN("[SyncTask] WiFi connection cancelled.");
                //Send CMD
                //ui->setLedPattern(LED_ERROR_WIFI);
                syncFailed = true;
                break;
            }
            vTaskDelay(500 / portTICK_PERIOD_MS);
            LOG_PRINT(".");
            retries++;
        }
    }

    if (WiFi.status() != WL_CONNECTED) {
        LOG_PRINTLN("\n[SyncTask] FATAL: WiFi connection failed.");
        LOG_PRINTLN("[SyncTask] Performing aggressive WiFi cleanup...");
        // SOLUCIÓN 2: Aggressive WiFi cleanup on connection failure
        WiFi.disconnect(true);  // Disconnect and turn off
        vTaskDelay(500 / portTICK_PERIOD_MS);
        WiFi.mode(WIFI_OFF);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        syncFailed = true;
    }

    if (!syncFailed && !g_cancelSync) {
        LOG_PRINTLN("[SyncTask] WiFi conectado. Esperando asignacion de DNS (DHCP)...");
        vTaskDelay(2000 / portTICK_PERIOD_MS); // Le damos 2 segundos al router
    }
    
    // 3. Login
    if (!syncFailed && !g_cancelSync) {
        LOG_PRINTLN("\n[SyncTask] WiFi connected. Logging in...");
        if (!supabaseClient->login(DEVICE_EMAIL, DEVICE_PASSWORD, accessToken)) {
            LOG_PRINTLN("[SyncTask] FATAL: Supabase login failed.");
            //Send CMD
            //ui->setLedPattern(LED_ERROR_DB);
            syncFailed = true;
        } else {
            LOG_PRINTLN("[SyncTask] Login successful.");
        }
    }

    // 4. Sincronizar Archivos
    if (!syncFailed && !g_cancelSync) {
        LOG_PRINTLN("[SyncTask] Taking storage mutex...");
        
        // Tomar el Mutex para la operación de lectura de archivos
        if (xSemaphoreTake(dataManager->getMutex(), portMAX_DELAY) == pdTRUE) {
            LOG_PRINTLN("[SyncTask] Got mutex. Starting file sync...");
            
            bool shouldRepeat = true;     // Flag para reintentar lectura de archivos
            bool hasErrorIn400Retry = false; // Flag para detectar 400 durante repetición
            
            // BUCLE PRINCIPAL: Se repite si hay archivos con error 400 que se limpian
            while (shouldRepeat && !syncFailed && !g_cancelSync) {
                LOG_PRINTLN("[SyncTask] ========== SYNC CYCLE START ==========");
                shouldRepeat = false;  // Por defecto, no hay repetición a menos que haya 400s
                
                File root = dataManager->openSessionDirectory();
                if (!root) {
                    LOG_PRINTLN("[SyncTask] Failed to open session directory.");
                    break;
                }
                
                File file = root.openNextFile();
                if (!file) {
                    LOG_PRINTLN("[SyncTask] No session files to sync.");
                    root.close();
                    break;
                }
                
                // BUCLE DE LOTES: Lee archivos en lotes de 5
                while (file && !syncFailed && !g_cancelSync) {
                    
                    // --- INICIO DEL LOTE ---
                    DynamicJsonDocument batchDoc(4096);
                    JsonArray p_items = batchDoc.createNestedArray("p_items");
                    String filePathsToDelete[SYNC_BATCH_SIZE];
                    int batchCount = 0;

                    // Llenar el lote con hasta 5 archivos
                    for (int i = 0; i < SYNC_BATCH_SIZE && file && !g_cancelSync; i++) {
                        String path = file.path();
                        String sessionJson = file.readString();
                        file.close();

                        DynamicJsonDocument tempDoc(512);
                        if (deserializeJson(tempDoc, sessionJson) == DeserializationError::Ok) {
                            p_items.add(tempDoc.as<JsonObject>());
                            filePathsToDelete[batchCount] = path;
                            batchCount++;
                        } else {
                            LOG_PRINTF("[SyncTask] ERROR: Corrupt JSON file, deleting: %s\n", path.c_str());
                            dataManager->deleteSessionFile(path);
                        }
                        file = root.openNextFile();
                    }

                    // Si el lote tiene items, subirlo
                    if (batchCount > 0 && !g_cancelSync) {
                        String batchJsonString;
                        serializeJson(batchDoc, batchJsonString);
                        
                        LOG_PRINTF("[SyncTask] [BATCH] Uploading %d sessions...\n", batchCount);
                        UploadResult uploadResult = supabaseClient->recordTrainingBatch(accessToken, batchJsonString);
                        
                        if (uploadResult == UPLOAD_SUCCESS) {
                            LOG_PRINTLN("[SyncTask] [BATCH] Upload successful. Deleting files...");
                            for (int i = 0; i < batchCount; i++) {
                                dataManager->deleteSessionFile(filePathsToDelete[i]);
                            }
                        } else if (uploadResult == UPLOAD_VALIDATION_ERROR) {
                            // HTTP 400: Validación - procesar cada archivo pero NO reintentar ahora
                            LOG_PRINTLN("[SyncTask] [BATCH] FAILED (400 - Validation Error). Validating and cleaning...");
                            
                            bool hasValidOrRecoverable = false;
                            
                            for (int i = 0; i < batchCount; i++) {
                                String filePath = filePathsToDelete[i];
                                LOG_PRINTF("\n[SyncTask] [VALIDATION] File: %s\n", filePath.c_str());
                                
                                File valFile = LittleFS.open(filePath, "r");
                                if (!valFile) {
                                    LOG_PRINTF("[SyncTask] ERROR: Cannot read file for validation: %s\n", filePath.c_str());
                                    continue;
                                }
                                
                                String fileContent = valFile.readString();
                                valFile.close();
                                
                                ValidationResult result = dataManager->validateSessionFile(fileContent);
                                
                                if (result == VALID) {
                                    LOG_PRINTF("[SyncTask] [VALIDATION] File VALID: %s (will retry in CYCLE 2)\n", filePath.c_str());
                                    hasValidOrRecoverable = true;
                                } else if (result == RECOVERABLE) {
                                    LOG_PRINTF("[SyncTask] [VALIDATION] File RECOVERABLE: %s (cleaning...)\n", filePath.c_str());
                                    if (dataManager->cleanAndSaveSessionFile(filePath)) {
                                        LOG_PRINTF("[SyncTask] [VALIDATION] File cleaned: %s (will retry in CYCLE 2)\n", filePath.c_str());
                                        hasValidOrRecoverable = true;
                                    } else {
                                        LOG_PRINTF("[SyncTask] [VALIDATION] Failed to clean: %s (deleting)\n", filePath.c_str());
                                        dataManager->deleteSessionFile(filePath);
                                    }
                                } else if (result == UNRECOVERABLE) {
                                    LOG_PRINTF("[SyncTask] [VALIDATION] File UNRECOVERABLE: %s (deleting)\n", filePath.c_str());
                                    dataManager->deleteSessionFile(filePath);
                                }
                            }
                            
                            LOG_PRINTLN("[SyncTask] [VALIDATION] Validation complete for this batch.");
                            
                            // Marcar para repetir si hay archivos válidos o limpios en este lote
                            if (hasValidOrRecoverable) {
                                shouldRepeat = true;
                                LOG_PRINTLN("[SyncTask] [VALIDATION] Found valid/cleaned files - will retry in CYCLE 2");
                            } else {
                                LOG_PRINTLN("[SyncTask] [VALIDATION] All files in this batch were UNRECOVERABLE (deleted)");
                            }
                        
                        // NO reintentar el lote ahora, continuar al siguiente lote
                        } else if (uploadResult == UPLOAD_TIMEOUT) {
                            LOG_PRINTLN("[SyncTask] [BATCH] FAILED (408 - Timeout)");
                            syncFailed = true;
                        } else if (uploadResult == UPLOAD_SERVER_ERROR || uploadResult == UPLOAD_UNAVAILABLE) {
                            LOG_PRINTF("[SyncTask] [BATCH] FAILED (%d - Server Error)\n", uploadResult);
                            syncFailed = true;
                        } else {
                            LOG_PRINTLN("[SyncTask] [BATCH] FAILED (Unknown Error)");
                            syncFailed = true;
                        }
                    }
                    
                    // Fin del lote, continuar con el siguiente
                }
                
                root.close();
                
                // Si hay que repetir, cerrar el directorio y volver a abrirlo
                if (shouldRepeat && !syncFailed && !g_cancelSync) {
                    LOG_PRINTLN("[SyncTask] ========== RESTARTING FILE SYNC ==========");
                    // El siguiente ciclo abrirá el directorio nuevamente
                }
            }
            
            LOG_PRINTLN("[SyncTask] Finished file sync. Releasing mutex.");
            if (hasErrorIn400Retry) {
                LOG_PRINTLN("[SyncTask] ERROR: 400 validation error during retry - aborting sync");
            }
            xSemaphoreGive(dataManager->getMutex());
        }
    }

    // 5. Sincronizar lista de perros
    if (!syncFailed && !g_cancelSync) {
        LOG_PRINTLN("[SyncTask] Fetching dog list...");
        String dogListJson;
        if (supabaseClient->listDogs(accessToken, dogListJson)) {
            dataManager->saveDogList(dogListJson);
            LOG_PRINTLN("[SyncTask] Dog list saved to LittleFS.");
        } else {
            LOG_PRINTLN("[SyncTask] Failed to fetch dog list.");
            syncFailed = true;
        }
    }

    // 6. Reportar a la FSM y limpiar
    Event finalEvent;
    if (syncFailed || g_cancelSync) {
        finalEvent.type = EVENT_SYNC_FAILED;
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    } 
    else{
        LOG_PRINTLN("[SyncTask] Sync process completed successfully.");
        finalEvent.type = EVENT_SYNC_COMPLETED;
    }
    
    // SOLUCIÓN 2: Clean WiFi stack before returning to ConfigState
    LOG_PRINTLN("[SyncTask] Cleaning WiFi stack...");
    WiFi.disconnect(true);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    WiFi.mode(WIFI_OFF);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    LOG_PRINTLN("[SyncTask] WiFi disconnected and turned off.");

    // Enviar evento final a la FSM
    xQueueSend(fsmQueue, &finalEvent, 0);

    // 7. Autodestruir la tarea
    LOG_PRINTLN("[SyncTask] Task self-deleting.");
    g_syncTaskRunning = false;  // Mark task as stopped before deletion
    vTaskDelete(NULL);
}

/****************************************************************
 * Class Method Implementations (SyncState)
 ****************************************************************/

SyncState::SyncState(DataManager* dataManager, SupabaseClient* supabaseClient) 
    : dataManager(dataManager), supabaseClient(supabaseClient), h_syncTask(NULL)
{
    // Constructor
}

void SyncState::enter(StateManager* manager) {
    LOG_PRINTLN("Entering SyncState... Launching background task.");
    //Send CMD
    manager->getHardwareManager()->sendCommand(CMD_LAUNCHER_OFF, 0);

    g_cancelSync = false; // Resetea la bandera de cancelación

    static SyncTaskParams params; 
    params.dataManager = this->dataManager;
    params.supabaseClient = this->supabaseClient;
    params.hardwareManager = manager->getHardwareManager();
    params.fsmQueue = manager->getEventQueue();

    // Lanza la tarea
    xTaskCreate(
        syncTaskFunction,
        "SyncTask",
        SYNC_TASK_STACK_SIZE,
        &params, // Pasa los parámetros
        SYNC_TASK_PRIORITY,
        &h_syncTask // Guarda el handle de la tarea
    );
}

void SyncState::execute(StateManager* manager) {
    Event event;
    QueueHandle_t queue = manager->getEventQueue();

    if (xQueueReceive(queue, &event, portMAX_DELAY) == pdTRUE) {
        handleEvent(manager, event);
    }
}

void SyncState::exit(StateManager* manager) {
    LOG_PRINTLN("Exiting SyncState...");
    
    // 1. Señalizar a la tarea que debe cancelarse
    if (h_syncTask != NULL) {
        LOG_PRINTLN("Sending cancel signal to SyncTask...");
        g_cancelSync = true;
        
        // 2. ESPERAR a que la tarea termine (máximo 5 segundos)
        int waitTimeout = 50;  // 50 * 100ms = 5 segundos
        while (g_syncTaskRunning && waitTimeout > 0) {
            vTaskDelay(100 / portTICK_PERIOD_MS);
            waitTimeout--;
        }
        
        if (g_syncTaskRunning) {
            LOG_PRINTLN("[WARNING] SyncTask did not terminate in time, forcing stop...");
            g_syncTaskRunning = false;  // Force the flag
        } else {
            LOG_PRINTLN("[SyncState] SyncTask terminated gracefully.");
        }
        
        h_syncTask = NULL;
    }
    
    // 3. Extra delay to ensure WiFi stack is completely shut down
    LOG_PRINTLN("[SyncState] Waiting for WiFi cleanup...");
    vTaskDelay(500 / portTICK_PERIOD_MS);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}


void SyncState::handleEvent(StateManager* manager, Event& event) {
    switch (event.type) {
        case EVENT_MODE_OFFLINE_ACTIVATED:
            LOG_PRINTLN("[SyncState] Event: Mode OFFLINE. Changing to ConfigState.");
            manager->changeState(new PowerUpState());
            break;

        case EVENT_SYNC_COMPLETED:
            LOG_PRINTLN("[SyncState] Event: Sync Completed. Changing to IdleState.");
            //Send CMD
            //manager->getUserInterface()->setLedPattern(LED_SUCCESS);
            manager->changeState(new IdleState());
            break;

        case EVENT_SYNC_FAILED:
            LOG_PRINTLN("[SyncState] Event: Sync FAILED. Changing to IdleState.");
            //Send CMD
            //manager->getUserInterface()->setLedPattern(LED_ERROR_DB);
            manager->changeState(new IdleState());
            break;
        
        case EVENT_USB_CONNECTED:
            LOG_PRINTLN("[SyncState] Event: USB Connected. Cancelling sync and changing to PowerOffState.");
            EVENT_WARN("Syn:USB->PwOff");
            manager->changeState(new PowerOffState());
            break;
            
        case EVENT_POWER_SWITCH_OFF:
            LOG_PRINTLN("[SyncState] Event: Power Switch OFF. Cancelling sync and changing to PowerOffState.");
            EVENT_WARN("Syn:PWR->PwOff");
            manager->changeState(new PowerOffState());
            break;
        
        default:
            break;
    }
}

void SyncState::update(StateManager* manager){
}
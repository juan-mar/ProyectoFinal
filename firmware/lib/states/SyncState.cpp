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
        vTaskDelay(4000 / portTICK_PERIOD_MS); // Le damos 2 segundos al router
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

    // 4. Sincronizar Chunks
    if (!syncFailed && !g_cancelSync) {
        LOG_PRINTLN("[SyncTask] Taking storage mutex...");

        if (xSemaphoreTake(dataManager->getMutex(), portMAX_DELAY) == pdTRUE) {
            LOG_PRINTLN("[SyncTask] Got mutex. Starting chunk sync...");

            bool isRetryPass = false;

            // Two-pass loop:
            //   Pass 1: upload every .log chunk; on 400 keep it.
            //   Pass 2 (only if pass 1 had any 400): retry kept chunks;
            //           on 400 again → delete (corrupt data).
            //   500/timeout at any point → abort immediately.
            while (!syncFailed && !g_cancelSync) {
                LOG_PRINTF("[SyncTask] ===== SYNC PASS %s =====\n",
                           isRetryPass ? "2 (RETRY)" : "1");
                LOG_PRINTF("[SyncTask] PASS MODE: %s\n",
                           isRetryPass ? "RAW + PER-ITEM FALLBACK" : "RAW (no local cleaning)");
                bool hadAny400ThisPass = false;

                File root = dataManager->openSessionDirectory();
                if (!root) {
                    LOG_PRINTLN("[SyncTask] Failed to open session directory.");
                    break;
                }

                File dirEntry = root.openNextFile();
                while (dirEntry && !syncFailed && !g_cancelSync) {
                    if (dirEntry.isDirectory()) {
                        dirEntry.close();
                        dirEntry = root.openNextFile();
                        continue;
                    }

                    String chunkPath = String(dirEntry.path());
                    dirEntry.close(); // free dir-entry slot before opening the file

                    String chunkName = chunkPath.substring(chunkPath.lastIndexOf('/') + 1);
                    if (!chunkName.endsWith(".log")) {
                        dirEntry = root.openNextFile();
                        continue;
                    }

                    LOG_PRINTF("[SyncTask] [CHUNK] %s (pass %s)\n",
                               chunkName.c_str(), isRetryPass ? "RETRY" : "1");

                    File chunkFile = LittleFS.open(chunkPath, "r");
                    if (!chunkFile) {
                        LOG_PRINTF("[SyncTask] Cannot open chunk: %s\n", chunkPath.c_str());
                        dirEntry = root.openNextFile();
                        continue;
                    }

                    // Per-line state in RAM (0=pending, 1=ok, 2=fail). Dies at end of chunk.
                    int state[MAX_SESSIONS_PER_CHUNK];
                    memset(state, 0, sizeof(state));
                    int totalLines = 0;

                    String batchLines[SYNC_BATCH_SIZE];
                    int    batchIndices[SYNC_BATCH_SIZE];
                    int    batchCount = 0;

                    // ---- Read lines, upload in batches of SYNC_BATCH_SIZE ----
                    while (chunkFile.available() && !syncFailed && !g_cancelSync) {
                        String line = chunkFile.readStringUntil('\n');
                        line.trim();
                        if (line.length() == 0) continue;
                        if (totalLines >= MAX_SESSIONS_PER_CHUNK) {
                            LOG_PRINTF("[SyncTask] WARNING: chunk exceeds %d lines: %s\n",
                                       MAX_SESSIONS_PER_CHUNK, chunkName.c_str());
                            break;
                        }

                        batchLines[batchCount]   = line;
                        batchIndices[batchCount] = totalLines;
                        batchCount++;
                        totalLines++;

                        if (batchCount == SYNC_BATCH_SIZE) {
                            DynamicJsonDocument batchDoc(4096);
                            JsonArray p_items = batchDoc.createNestedArray("p_items");
                            for (int bi = 0; bi < batchCount; bi++) {
                                DynamicJsonDocument lineDoc(512);
                                // RAW flow in both passes: Supabase decides recoverable/non-recoverable.
                                if (deserializeJson(lineDoc, batchLines[bi]) == DeserializationError::Ok) {
                                    p_items.add(lineDoc.as<JsonObject>());
                                } else {
                                    state[batchIndices[bi]] = 2;
                                    hadAny400ThisPass = true;
                                    LOG_PRINTF("[SyncTask] Corrupt line %d in %s\n",
                                               batchIndices[bi], chunkName.c_str());
                                }
                            }
                            if (p_items.size() > 0) {
                                String batchJson;
                                serializeJson(batchDoc, batchJson);
                                LOG_PRINTF("[SyncTask] Uploading %d sessions...\n",
                                           (int)p_items.size());
                                UploadResult result = supabaseClient->recordTrainingBatch(
                                    accessToken, batchJson);
                                if (result == UPLOAD_SUCCESS) {
                                    for (int bi = 0; bi < batchCount; bi++)
                                        if (state[batchIndices[bi]] != 2)
                                            state[batchIndices[bi]] = 1;
                                } else if (result == UPLOAD_VALIDATION_ERROR) {
                                    LOG_PRINTF("[SyncTask] 400 on batch from %s (pass %s)\n",
                                               chunkName.c_str(), isRetryPass ? "RETRY" : "1");
                                    hadAny400ThisPass = true;

                                    if (!isRetryPass) {
                                        for (int bi = 0; bi < batchCount; bi++) {
                                            if (state[batchIndices[bi]] != 2) {
                                                state[batchIndices[bi]] = 2;
                                            }
                                        }
                                    } else {
                                        // Retry pass: salvage valid rows with per-item fallback.
                                        for (int bi = 0; bi < batchCount && !syncFailed && !g_cancelSync; bi++) {
                                            int idx = batchIndices[bi];
                                            if (state[idx] == 2) {
                                                continue;
                                            }

                                            DynamicJsonDocument oneItemDoc(1024);
                                            JsonArray oneItemArray = oneItemDoc.createNestedArray("p_items");
                                            DynamicJsonDocument lineDoc(512);
                                            if (deserializeJson(lineDoc, batchLines[bi]) != DeserializationError::Ok) {
                                                state[idx] = 2;
                                                continue;
                                            }
                                            oneItemArray.add(lineDoc.as<JsonObject>());

                                            String oneItemJson;
                                            serializeJson(oneItemDoc, oneItemJson);
                                            UploadResult oneResult = supabaseClient->recordTrainingBatch(accessToken, oneItemJson);
                                            if (oneResult == UPLOAD_SUCCESS) {
                                                state[idx] = 1;
                                                LOG_PRINTF("[SyncTask] Retry salvage OK line %d in %s\n",
                                                           idx, chunkName.c_str());
                                            } else if (oneResult == UPLOAD_VALIDATION_ERROR) {
                                                state[idx] = 2;
                                                LOG_PRINTF("[SyncTask] Retry salvage FAILED line %d in %s\n",
                                                           idx, chunkName.c_str());
                                            } else {
                                                LOG_PRINTF("[SyncTask] Retry salvage fatal error line %d in %s\n",
                                                           idx, chunkName.c_str());
                                                syncFailed = true;
                                            }
                                        }
                                    }
                                } else {
                                    LOG_PRINTF("[SyncTask] Fatal error on %s -> aborting\n",
                                               chunkName.c_str());
                                    syncFailed = true;
                                }
                            }
                            batchCount = 0;
                        }
                    }

                    // ---- Last partial batch ----
                    if (batchCount > 0 && !syncFailed && !g_cancelSync) {
                        DynamicJsonDocument batchDoc(4096);
                        JsonArray p_items = batchDoc.createNestedArray("p_items");
                        for (int bi = 0; bi < batchCount; bi++) {
                            DynamicJsonDocument lineDoc(512);
                            // RAW flow in both passes: Supabase decides recoverable/non-recoverable.
                            if (deserializeJson(lineDoc, batchLines[bi]) == DeserializationError::Ok) {
                                p_items.add(lineDoc.as<JsonObject>());
                            } else {
                                state[batchIndices[bi]] = 2;
                                hadAny400ThisPass = true;
                                LOG_PRINTF("[SyncTask] Corrupt line %d in %s\n",
                                           batchIndices[bi], chunkName.c_str());
                            }
                        }
                        if (p_items.size() > 0) {
                            String batchJson;
                            serializeJson(batchDoc, batchJson);
                            LOG_PRINTF("[SyncTask] Uploading last %d sessions...\n",
                                       (int)p_items.size());
                            UploadResult result = supabaseClient->recordTrainingBatch(
                                accessToken, batchJson);
                            if (result == UPLOAD_SUCCESS) {
                                for (int bi = 0; bi < batchCount; bi++)
                                    if (state[batchIndices[bi]] != 2)
                                        state[batchIndices[bi]] = 1;
                            } else if (result == UPLOAD_VALIDATION_ERROR) {
                                LOG_PRINTF("[SyncTask] 400 on last batch from %s (pass %s)\n",
                                           chunkName.c_str(), isRetryPass ? "RETRY" : "1");
                                hadAny400ThisPass = true;

                                if (!isRetryPass) {
                                    for (int bi = 0; bi < batchCount; bi++) {
                                        if (state[batchIndices[bi]] != 2) {
                                            state[batchIndices[bi]] = 2;
                                        }
                                    }
                                } else {
                                    // Retry pass: salvage valid rows with per-item fallback.
                                    for (int bi = 0; bi < batchCount && !syncFailed && !g_cancelSync; bi++) {
                                        int idx = batchIndices[bi];
                                        if (state[idx] == 2) {
                                            continue;
                                        }

                                        DynamicJsonDocument oneItemDoc(1024);
                                        JsonArray oneItemArray = oneItemDoc.createNestedArray("p_items");
                                        DynamicJsonDocument lineDoc(512);
                                        if (deserializeJson(lineDoc, batchLines[bi]) != DeserializationError::Ok) {
                                            state[idx] = 2;
                                            continue;
                                        }
                                        oneItemArray.add(lineDoc.as<JsonObject>());

                                        String oneItemJson;
                                        serializeJson(oneItemDoc, oneItemJson);
                                        UploadResult oneResult = supabaseClient->recordTrainingBatch(accessToken, oneItemJson);
                                        if (oneResult == UPLOAD_SUCCESS) {
                                            state[idx] = 1;
                                            LOG_PRINTF("[SyncTask] Retry salvage OK line %d in %s\n",
                                                       idx, chunkName.c_str());
                                        } else if (oneResult == UPLOAD_VALIDATION_ERROR) {
                                            state[idx] = 2;
                                            LOG_PRINTF("[SyncTask] Retry salvage FAILED line %d in %s\n",
                                                       idx, chunkName.c_str());
                                        } else {
                                            LOG_PRINTF("[SyncTask] Retry salvage fatal error line %d in %s\n",
                                                       idx, chunkName.c_str());
                                            syncFailed = true;
                                        }
                                    }
                                }
                            } else {
                                LOG_PRINTF("[SyncTask] Fatal error on last batch of %s -> aborting\n",
                                           chunkName.c_str());
                                syncFailed = true;
                            }
                        }
                        batchCount = 0;
                    }

                    chunkFile.close();

                    // ---- Decide what to do with this chunk ----
                    if (!syncFailed) {
                        bool allOk = true;
                        for (int i = 0; i < totalLines; i++) {
                            if (state[i] != 1) { allOk = false; break; }
                        }
                        if (allOk) {
                            LOG_PRINTF("[SyncTask] All lines OK -> deleting %s\n",
                                       chunkName.c_str());
                            dataManager->deleteSessionFile(chunkPath);
                        } else if (isRetryPass) {
                            // Still failing on retry -> corrupt data -> delete
                            LOG_PRINTF("[SyncTask] Failed on retry (corrupt) -> deleting %s\n",
                                       chunkName.c_str());
                            dataManager->deleteSessionFile(chunkPath);
                        } else {
                            LOG_PRINTF("[SyncTask] Has failures -> will retry: %s\n",
                                       chunkName.c_str());
                        }
                    }

                    dirEntry = root.openNextFile();
                } // end while (dirEntry)

                root.close();

                if (syncFailed || g_cancelSync) break;
                // If no 400s this pass, or we already did the retry pass -> done
                if (!hadAny400ThisPass || isRetryPass) break;
                // Pass 1 had 400s -> start retry pass
                isRetryPass = true;
                LOG_PRINTLN("[SyncTask] 400 errors in pass 1 -> starting retry pass...");

            } // end while (two-pass loop)

            LOG_PRINTLN("[SyncTask] Finished chunk sync. Releasing mutex.");
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
    manager->getHardwareManager()->sendCommand(CMD_MSG_SET, USER_MSG_SYNCING);
    manager->getHardwareManager()->sendCommand(CMD_LAUNCHER_OFF, 0);

    g_cancelSync = false; // Resetea la bandera de cancelación

    static SyncTaskParams params; 
    params.dataManager = this->dataManager;
    params.supabaseClient = this->supabaseClient;
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
            manager->getHardwareManager()->sendCommand(CMD_MSG_SET, USER_MSG_SUCCESS);
            vTaskDelay(pdMS_TO_TICKS(350));
            manager->changeState(new IdleState());
            break;

        case EVENT_SYNC_FAILED:
            LOG_PRINTLN("[SyncState] Event: Sync FAILED. Changing to IdleState.");
            manager->getHardwareManager()->sendCommand(CMD_MSG_SET, USER_MSG_ERROR);
            vTaskDelay(pdMS_TO_TICKS(500));
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
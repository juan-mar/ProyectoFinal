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

#include "Events.h"
#include "config.h"
#include "credentials.h"

#include <WiFi.h>
#include <ArduinoJson.h>

// States we can transition to
#include "ConfigState.h"
#include "IdleState.h"

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

/****************************************************************
 * Background Sync Task (La Lógica "Real")
 ****************************************************************/

/**
 * @brief The actual background task function that does the work.
 */
void syncTaskFunction(void* parameter) {
    LOG_PRINTLN("[SyncTask] Task started.");
    
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
            LOG_PRINTLN("[SyncTask] Got mutex. Opening session directory...");
            File root = dataManager->openSessionDirectory();
            
            if (!root) {
                LOG_PRINTLN("[SyncTask] Failed to open session directory.");
            } else {
                File file = root.openNextFile();
                if (!file) {
                    LOG_PRINTLN("[SyncTask] No session files to sync.");
                }

                // Bucle principal: se ejecuta mientras haya archivos
                while (file && !syncFailed && !g_cancelSync) {
                    
                    // --- INICIO DEL LOTE ---
                    DynamicJsonDocument batchDoc(4096); // Búfer grande en HEAP
                    JsonArray p_items = batchDoc.createNestedArray("p_items");
                    String filePathsToDelete[SYNC_BATCH_SIZE];
                    int batchCount = 0;

                    // Llenar el lote
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
                            dataManager->deleteSessionFile(path); // Borra el archivo corrupto
                        }
                        file = root.openNextFile();
                    }

                    // Si el lote tiene items, subirlo
                    if (batchCount > 0 && !g_cancelSync) {
                        String batchJsonString;
                        serializeJson(batchDoc, batchJsonString);
                        
                        LOG_PRINTF("[SyncTask] Uploading batch of %d sessions...\n", batchCount);
                        if (supabaseClient->recordTrainingBatch(accessToken, batchJsonString)) {
                            LOG_PRINTLN("[SyncTask] Batch upload successful. Deleting files...");
                            for (int i = 0; i < batchCount; i++) {
                                dataManager->deleteSessionFile(filePathsToDelete[i]);
                            }
                        } else {
                            LOG_PRINTLN("[SyncTask] Batch upload FAILED. Stopping sync.");
                            syncFailed = true;
                        }
                    }
                }
                root.close();
            }
            LOG_PRINTLN("[SyncTask] Finished batch sync. Releasing mutex.");
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
    
    // Desconectar WiFi
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    LOG_PRINTLN("[SyncTask] WiFi disconnected.");

    // Enviar evento final a la FSM
    xQueueSend(fsmQueue, &finalEvent, 0);

    // 7. Autodestruir la tarea
    LOG_PRINTLN("[SyncTask] Task self-deleting.");
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
    }

    // 2. Apagar el WiFi (aunque la tarea ya debería haberlo hecho)
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}


void SyncState::handleEvent(StateManager* manager, Event& event) {
    switch (event.type) {
        case EVENT_MODE_OFFLINE_ACTIVATED:
            LOG_PRINTLN("[SyncState] Event: Mode OFFLINE. Changing to ConfigState.");
            manager->changeState(new ConfigState(dataManager, manager->getWebServerManager()));
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
        
        default:
            break;
    }
}

void SyncState::update(StateManager* manager){
}
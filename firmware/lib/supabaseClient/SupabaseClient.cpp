/****************************************************************
 * @file SupabaseClient.cpp
 * @brief Implements the SupabaseClient class methods.
 ****************************************************************/

/****************************************************************
 * Headers
 ****************************************************************/
#include "SupabaseClient.h"
#include "config.h" // For logging
#include <WiFi.h>

/****************************************************************
 * Class Method Implementations
 ****************************************************************/

static const char* wifiStatusToString(wl_status_t status) {
    switch (status) {
        case WL_NO_SHIELD: return "NO_SHIELD";
        case WL_IDLE_STATUS: return "IDLE";
        case WL_NO_SSID_AVAIL: return "NO_SSID";
        case WL_SCAN_COMPLETED: return "SCAN_COMPLETED";
        case WL_CONNECTED: return "CONNECTED";
        case WL_CONNECT_FAILED: return "CONNECT_FAILED";
        case WL_CONNECTION_LOST: return "CONNECTION_LOST";
        case WL_DISCONNECTED: return "DISCONNECTED";
        default: return "UNKNOWN";
    }
}

static void logSupabaseTransportContext(const char* operation, const String& url) {
    LOG_PRINTF("[Supabase][%s] URL=%s\n", operation, url.c_str());
    LOG_PRINTF("[Supabase][%s] WiFi status=%s (%d) connected=%s\n",
               operation,
               wifiStatusToString(WiFi.status()),
               static_cast<int>(WiFi.status()),
               WiFi.isConnected() ? "YES" : "NO");
    if (WiFi.isConnected()) {
        LOG_PRINTF("[Supabase][%s] IP=%s RSSI=%d dBm\n",
                   operation,
                   WiFi.localIP().toString().c_str(),
                   WiFi.RSSI());
    }
}

SupabaseClient::SupabaseClient(const char* baseUrl, const char* apiKey)
    : supabaseUrl(baseUrl), supabaseApiKey(apiKey)
{
    // Use an explicit TLS client so HTTPS handshakes do not depend on
    // certificate provisioning or a synchronized clock.
    secureClient.setInsecure();
}

bool SupabaseClient::login(String email, String password, String &outAccessToken) {
    String url = supabaseUrl + "/auth/v1/token?grant_type=password";

    StaticJsonDocument<1024> doc; // Búfer grande para la respuesta
    
    // Preparar el body del envío
    doc["email"] = email;
    doc["password"] = password;
    
    String jsonBody;
    serializeJson(doc, jsonBody);

    LOG_PRINTF("[Supabase][login] Request body length=%u\n", static_cast<unsigned>(jsonBody.length()));
    logSupabaseTransportContext("login", url);

    if (!http.begin(secureClient, url)) {
        LOG_PRINTLN("[Supabase][login] ERROR: http.begin failed");
        return false;
    }

    http.setTimeout(15000); // 15 segundos timeout
    http.addHeader("apikey", supabaseApiKey);
    http.addHeader("Authorization", "Bearer " + supabaseApiKey);
    http.addHeader("Content-Type", "application/json");

    unsigned long startMs = millis();
    int httpCode = http.POST(jsonBody);
    unsigned long elapsedMs = millis() - startMs;
    LOG_PRINTF("[Supabase][login] HTTP Code=%d elapsed=%lums\n", httpCode, elapsedMs);
    bool success = false;

    if (httpCode == 200) {
        String payload = http.getString();
        LOG_PRINTF("[Supabase][login] Response length=%u\n", static_cast<unsigned>(payload.length()));
        
        StaticJsonDocument<32> filter;
        filter["access_token"] = true;
        doc.clear();
        
        DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
        if (!error) {
            outAccessToken = doc["access_token"].as<String>();
            success = true;
        } else {
            LOG_PRINTF("SupabaseClient::login Failed to parse JSON. Error: %s\n", error.c_str());
        }
    } else {
        LOG_PRINTF("SupabaseClient::login HTTP Error: %d\n", httpCode);
        LOG_PRINTLN(http.getString());
    }

    http.end();
    return success;
}


bool SupabaseClient::listDogs(String accessToken, String &outJsonString) {
    String url = supabaseUrl + "/rest/v1/dogs";
    String params = "?select=*&active=eq.true&order=created_at.desc";

    String fullUrl = url + params;
    logSupabaseTransportContext("listDogs", fullUrl);

    if (!http.begin(secureClient, fullUrl)) {
        LOG_PRINTLN("[Supabase][listDogs] ERROR: http.begin failed");
        return false;
    }

    http.setTimeout(15000); // 15 segundos timeout
    http.addHeader("apikey", supabaseApiKey);
    http.addHeader("Authorization", "Bearer " + accessToken);

    unsigned long startMs = millis();
    int httpCode = http.GET();
    unsigned long elapsedMs = millis() - startMs;
    LOG_PRINTF("[Supabase][listDogs] HTTP Code=%d elapsed=%lums\n", httpCode, elapsedMs);
    bool success = false;
    
    if (httpCode == 200) {
        outJsonString = http.getString();
        LOG_PRINTF("[Supabase][listDogs] Response length=%u\n", static_cast<unsigned>(outJsonString.length()));
        success = true;
        LOG_PRINTLN("[Supabase] Perros descargados con exito. Payload:");
        LOG_PRINTLN(outJsonString);
    } else {
        LOG_PRINTF("SupabaseClient::listDogs HTTP Error: %d\n", httpCode);
        LOG_PRINTLN(http.getString());
    }

    http.end();
    return success;
}


UploadResult SupabaseClient::recordTrainingBatch(String accessToken, String batchJsonString) {
    String url = supabaseUrl + "/rest/v1/rpc/record_training_batch";

    LOG_PRINTLN("\n--- ENVIANDO A SUPABASE ---");
    LOG_PRINTLN(batchJsonString);
    LOG_PRINTLN("---------------------------");
    LOG_PRINTF("[Supabase][recordTrainingBatch] Payload length=%u accessToken length=%u\n",
               static_cast<unsigned>(batchJsonString.length()),
               static_cast<unsigned>(accessToken.length()));
    logSupabaseTransportContext("recordTrainingBatch", url);

    if (!http.begin(secureClient, url)) {
        LOG_PRINTLN("[Supabase][recordTrainingBatch] ERROR: http.begin failed");
        http.end();
        return UPLOAD_UNKNOWN_ERROR;
    }

    http.setTimeout(15000); // 15 segundos timeout
    http.addHeader("apikey", supabaseApiKey);
    http.addHeader("Authorization", "Bearer " + accessToken);
    http.addHeader("Content-Type", "application/json");

    unsigned long startMs = millis();
    int httpCode = http.POST(batchJsonString);
    unsigned long elapsedMs = millis() - startMs;
    
    LOG_PRINTF("[Supabase] HTTP Code: %d elapsed=%lums\n", httpCode, elapsedMs);
    
    UploadResult result;
    
    // Manejar timeout del cliente (código negativo o -1)
    if (httpCode <= 0) {
        result = UPLOAD_TIMEOUT;
        LOG_PRINTLN("[Supabase] ERROR: Connection timeout or failed");
        LOG_PRINTF("[Supabase][recordTrainingBatch] WiFi status=%s (%d) connected=%s\n",
                   wifiStatusToString(WiFi.status()),
                   static_cast<int>(WiFi.status()),
                   WiFi.isConnected() ? "YES" : "NO");
        http.end();
        return result;
    }
    
    // Solo leer respuesta si hay contenido (204 = No Content)
    if (httpCode != 204) {
        String response = http.getString(); 
        LOG_PRINTF("[Supabase][recordTrainingBatch] Response length=%u\n",
                   static_cast<unsigned>(response.length()));
        LOG_PRINTLN("[Supabase] Respuesta del Servidor: " + response);
    }
    
    if (httpCode == 200 || httpCode == 204) {
        result = UPLOAD_SUCCESS;
    } else if (httpCode == 400) {
        result = UPLOAD_VALIDATION_ERROR;
        LOG_PRINTLN("[Supabase] ERROR 400: Data validation error");
    } else if (httpCode == 408) {
        result = UPLOAD_TIMEOUT;
        LOG_PRINTLN("[Supabase] ERROR 408: Request timeout");
    } else if (httpCode == 500) {
        result = UPLOAD_SERVER_ERROR;
        LOG_PRINTLN("[Supabase] ERROR 500: Server error");
    } else if (httpCode == 503) {
        result = UPLOAD_UNAVAILABLE;
        LOG_PRINTLN("[Supabase] ERROR 503: Service unavailable");
    } else {
        result = UPLOAD_UNKNOWN_ERROR;
        LOG_PRINTF("[Supabase] ERROR: Unknown HTTP code %d\n", httpCode);
    }

    http.end();
    return result;
}

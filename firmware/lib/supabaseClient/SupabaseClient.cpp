/****************************************************************
 * @file SupabaseClient.cpp
 * @brief Implements the SupabaseClient class methods.
 ****************************************************************/

/****************************************************************
 * Headers
 ****************************************************************/
#include "SupabaseClient.h"
#include "config.h" // For logging

/****************************************************************
 * Class Method Implementations
 ****************************************************************/

SupabaseClient::SupabaseClient(const char* baseUrl, const char* apiKey)
    : supabaseUrl(baseUrl), supabaseApiKey(apiKey)
{
    // Constructor stores the base URL and API key
}

bool SupabaseClient::login(String email, String password, String &outAccessToken) {
    String url = supabaseUrl + "/auth/v1/token?grant_type=password";

    StaticJsonDocument<1024> doc; // Búfer grande para la respuesta
    
    // Preparar el body del envío
    doc["email"] = email;
    doc["password"] = password;
    
    String jsonBody;
    serializeJson(doc, jsonBody);

    http.begin(url);
    http.setTimeout(15000); // 15 segundos timeout
    http.addHeader("apikey", supabaseApiKey);
    http.addHeader("Authorization", "Bearer " + supabaseApiKey);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(jsonBody);
    bool success = false;

    if (httpCode == 200) {
        String payload = http.getString();
        
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

    http.begin(url + params);
    http.setTimeout(15000); // 15 segundos timeout
    http.addHeader("apikey", supabaseApiKey);
    http.addHeader("Authorization", "Bearer " + accessToken);

    int httpCode = http.GET();
    bool success = false;
    
    if (httpCode == 200) {
        outJsonString = http.getString();
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

    http.begin(url);
    http.setTimeout(15000); // 15 segundos timeout
    http.addHeader("apikey", supabaseApiKey);
    http.addHeader("Authorization", "Bearer " + accessToken);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(batchJsonString);
    
    LOG_PRINTF("[Supabase] HTTP Code: %d\n", httpCode);
    
    UploadResult result;
    
    // Manejar timeout del cliente (código negativo o -1)
    if (httpCode <= 0) {
        result = UPLOAD_TIMEOUT;
        LOG_PRINTLN("[Supabase] ERROR: Connection timeout or failed");
        http.end();
        return result;
    }
    
    // Solo leer respuesta si hay contenido (204 = No Content)
    if (httpCode != 204) {
        String response = http.getString(); 
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

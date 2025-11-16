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
    http.addHeader("apikey", supabaseApiKey);
    http.addHeader("Authorization", "Bearer " + accessToken);

    int httpCode = http.GET();
    bool success = false;
    
    if (httpCode == 200) {
        outJsonString = http.getString();
        success = true;
    } else {
        LOG_PRINTF("SupabaseClient::listDogs HTTP Error: %d\n", httpCode);
        LOG_PRINTLN(http.getString());
    }

    http.end();
    return success;
}


bool SupabaseClient::recordTrainingBatch(String accessToken, String batchJsonString) {
    String url = supabaseUrl + "/rest/v1/rpc/record_training_batch";

    http.begin(url);
    http.addHeader("apikey", supabaseApiKey);
    http.addHeader("Authorization", "Bearer " + accessToken);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(batchJsonString);
    bool success = false;
    
    // Para RPC de batch, 200 (OK) o 204 (No Content) son éxito
    if (httpCode == 200 || httpCode == 204) {
        success = true;
    } else {
        LOG_PRINTF("SupabaseClient::recordBatch HTTP Error: %d\n", httpCode);
        LOG_PRINTLN(http.getString());
    }

    http.end();
    return success;
}

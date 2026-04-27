/****************************************************************
 * @file SupabaseClient.h
 * @brief Declares the SupabaseClient class, which handles all
 * HTTP communications with the Supabase backend.
 ****************************************************************/

/****************************************************************
 * Include Guards
 ****************************************************************/
#ifndef SUPABASE_CLIENT_H
#define SUPABASE_CLIENT_H

/****************************************************************
 * Headers
 ****************************************************************/
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

/****************************************************************
 * Enums
 ****************************************************************/

enum UploadResult {
    UPLOAD_SUCCESS = 0,       // HTTP 200/204
    UPLOAD_VALIDATION_ERROR = 400,  // Bad Request (data validation error)
    UPLOAD_TIMEOUT = 408,     // Request Timeout
    UPLOAD_SERVER_ERROR = 500, // Server Error
    UPLOAD_UNAVAILABLE = 503,  // Service Unavailable
    UPLOAD_UNKNOWN_ERROR = -1  // Other error
};

/****************************************************************
 * Class Declarations
 ****************************************************************/

/**
 * @brief Handles all HTTP API calls to Supabase (Auth, RPC, GET).
 */
class SupabaseClient {
public:
    /**
     * @brief Constructor.
     * @param baseUrl The base URL for the Supabase project (e.g., "https://...co").
     * @param apiKey The public 'anon' API key.
     */
    SupabaseClient(const char* baseUrl, const char* apiKey);

    /**
     * @brief Attempts to log in as the device.
     * @param email The device's email.
     * @param password The device's password.
     * @param outAccessToken (out) The string to store the access token in.
     * @return true if login was successful.
     */
    bool login(String email, String password, String &outAccessToken);

    /**
     * @brief Fetches the list of active dogs.
     * @param accessToken The valid JWT for the session.
     * @param outJsonString (out) The string to store the JSON response.
     * @return true if fetch was successful (HTTP 200).
     */
    bool listDogs(String accessToken, String &outJsonString);
    
    /**
     * @brief Calls the 'record_training_batch' RPC to upload sessions.
     * @param accessToken The valid JWT for the session.
     * @param batchJsonString The JSON string of the batch ({"p_items": [...]}).
     * @return UploadResult with HTTP code information.
     */
    UploadResult recordTrainingBatch(String accessToken, String batchJsonString);

private:
    String supabaseUrl;
    String supabaseApiKey;
    WiFiClientSecure secureClient;
    HTTPClient http;
};

#endif // SUPABASE_CLIENT_H
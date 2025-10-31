/****************************************************************
 * @file main.cpp
 * @brief Supabase Testbench for ESP32
 * Testbench for logging into Supabase Auth to get an access token.
 ****************************************************************/

/****************************************************************
 * Headers
 ****************************************************************/
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Includes user credentials from the include/ folder
#include "credentials.h"

/****************************************************************
 * Global Variables
 ****************************************************************/
String g_accessToken = "";

/****************************************************************
 * Function Prototypes
 ****************************************************************/
/**
 * @brief Connects to the WiFi network. Blocks until connected.
 */
void connectWiFi();

/**
 * @brief Attempts to log in to Supabase Auth.
 * @param email The user's email.
 * @param password The user's password.
 * @return A String with the access_token (if successful) or empty.
 */
String supabaseLogin(String email, String password);

/**
 * @brief Fetches a list of dogs from the 'dogs' table.
 * @param accessToken The JWT access token from login.
 * @param onlyActive If true, adds the 'active=eq.true' filter.
 * @param limit The max number of rows to return.
 * @return The HTTP response code.
 */
int listDogs(String accessToken, bool onlyActive = true, int limit = 100);

/**
 * @brief (Test 3) Calls an RPC in Supabase to record a training session.
 * @param accessToken The JWT access token from login.
 * @return The HTTP response code.
 */
int recordTrainingSession(String accessToken);

/**
 * @brief (Test 4) Calls a batch RPC to record multiple sessions.
 * @param accessToken The JWT access token from login.
 * @return The HTTP response code.
 */
int recordTrainingBatch(String accessToken);

/****************************************************************
 * Setup Function
 ****************************************************************/
void setup() {
    Serial.begin(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.println("\n--- Supabase Login Sandbox ---");
    
    // Connect to WiFi using credentials from credentials.h
    connectWiFi();
    
    Serial.println("\nSetup complete. Ready for testing.");
    Serial.println("Send commands via Serial Monitor (No new line/CR):");
    Serial.println(" 'l' -> Test LOGIN to Supabase Auth");
    Serial.println(" 'd' -> Test LIST DOGS (must login first)");
    Serial.println(" 'r' -> Test RECORD SESSION (must login first)");
    Serial.println(" 'b' -> Test RECORD BATCH (must login first)");
}

/****************************************************************
 * Loop Function (Test Trigger)
 ****************************************************************/
void loop() {
    // Wait for a test trigger from the Serial Monitor
    if (Serial.available() > 0) {
        char command = Serial.read();

     if (command == 'l') {
            // --- Test Login ---
            Serial.println("\n[Test 'l'] Received. Attempting login...");
            g_accessToken = supabaseLogin("lanzador_01@device.test", "lanzador_01");
            
            if (g_accessToken.length() > 0) {
                Serial.println("Login test successful. Token stored.");
            } else {
                Serial.println("Login test failed.");
            }
        
        } else if (command == 'd') {
            // --- Test List Dogs ---
            // el usuario de login debe ser previamente un admin
            if (g_accessToken.length() == 0) {
                Serial.println("\n[Test 'd'] Error: You must log in first! (Press 'l')");
            } else {
                Serial.println("\n[Test 'd'] Received. Listing dogs...");
                listDogs(g_accessToken);
            }
        
        } else if (command == 'r') {
            // --- Test Record Session ---
            // el usuario de login debe ser previamente un device
            if (g_accessToken.length() == 0) {
                Serial.println("\n[Test 'r'] Error: You must log in first! (Press 'l')");
            } else {
                Serial.println("\n[Test 'r'] Received. Recording hardcoded session...");
                recordTrainingSession(g_accessToken);
            }
        
        } else if (command == 'b') {
            // --- Test Record Batch ---
            // el usuario de login debe ser previamente un device
            if (g_accessToken.length() == 0) {
                Serial.println("\n[Test 'b'] Error: You must log in first! (Press 'l')");
            } else {
                Serial.println("\n[Test 'b'] Received. Recording hardcoded batch...");
                recordTrainingBatch(g_accessToken);
            }
        }
    }
    
    vTaskDelay(100 / portTICK_PERIOD_MS);
}

/****************************************************************
 * Helper Function Implementations
 ****************************************************************/

void connectWiFi() {
    Serial.print("Connecting to WiFi: ");
    Serial.print(WIFI_SSID); // From credentials.h

    WiFi.begin(WIFI_SSID, WIFI_PASS); // From credentials.h
    
    while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
        Serial.print(".");
    }
    
    Serial.println("\nWiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
}

// --- Login Function (FIXED with JSON Filter) ---
String supabaseLogin(String email, String password) {
    
    String url = String(SUPABASE_URL) + "/auth/v1/token?grant_type=password";

    // Mantenemos 1024. El filtro hará que esto sea suficiente.
    StaticJsonDocument<1024> doc;
    
    // Prepara el body del envío
    doc["email"] = email;
    doc["password"] = password;
    
    String jsonBody;
    serializeJson(doc, jsonBody);

    HTTPClient http;
    http.begin(url);
    http.addHeader("apikey", SUPABASE_API_KEY);
    http.addHeader("Authorization", String("Bearer ") + SUPABASE_API_KEY);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(jsonBody);
    String accessToken = "";

    if (httpCode == 200) {
        Serial.println("Login successful! (HTTP 200)");
        String payload = http.getString();
        
        Serial.println("--- RAW JSON RESPONSE ---");
        Serial.println(payload);
        Serial.println("-------------------------");

        // --- 1. Crear el Filtro ---
        StaticJsonDocument<32> filter;
        filter["access_token"] = true; 

        // Limpiamos el 'doc' antes de reutilizarlo
        doc.clear();

        // --- 2. Deserializar USANDO el Filtro ---
        DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
        
        if (!error) {
            // --- 3. Leer el dato filtrado ---
            accessToken = doc["access_token"].as<String>();
        } else {
            Serial.print("Failed to parse JSON response. Error: ");
            Serial.println(error.c_str()); // Debería desaparecer el NoMemory
        }
        
    } else {
        Serial.printf("[HTTP] Error on login POST: %d\n", httpCode);
        String payload = http.getString();
        Serial.println("Error response: " + payload);
    }

    http.end();
    return accessToken;
}

int listDogs(String accessToken, bool onlyActive, int limit) {
    
    // 1. Construir la URL con parámetros (el "params=..." de Python)
    String url = String(SUPABASE_URL) + "/rest/v1/dogs";
    String params = "?select=*";
    params += "&order=created_at.desc";
    params += "&limit=" + String(limit);
    if (onlyActive) {
        params += "&active=eq.true";
    }

    HTTPClient http;
    http.begin(url + params); // Combina la URL base y los parámetros

    // 2. Añadir los Headers
    // ¡Aquí usamos el accessToken que obtuvimos del login!
    http.addHeader("apikey", SUPABASE_API_KEY);
    http.addHeader("Authorization", String("Bearer ") + accessToken);

    // 3. Enviar la petición GET
    int httpCode = http.GET();
    
    if (httpCode == 200) { // 200 OK
        Serial.println("Dog list received successfully!");
        
        // Usamos un DynamicJsonDocument porque no sabemos
        // qué tan grande será la lista de perros.
        // 4096 bytes = 4KB. Ajusta si es necesario.
        DynamicJsonDocument doc(4096);
        
        String payload = http.getString();
        DeserializationError error = deserializeJson(doc, payload);

        if (error) {
            Serial.print("Failed to parse JSON response. Error: ");
            Serial.println(error.c_str());
        } else {
            // 4. Imprimir el JSON "bonito" (como el indent=2 de Python)
            Serial.println("--- List Dogs (Pretty JSON) ---");
            serializeJsonPretty(doc, Serial);
            Serial.println(); // Salto de línea extra
            Serial.println("-----------------------------");
        }
        
    } else {
        Serial.printf("[HTTP] Error on GET: %d\n", httpCode);
        String payload = http.getString();
        Serial.println("Error response: " + payload);
    }

    http.end();
    return httpCode;
}

int recordTrainingSession(String accessToken) {

    // 1. Construir la URL de la RPC
    String url = String(SUPABASE_URL) + "/rest/v1/rpc/record_training";

    // 2. Construir el JSON del Payload (los parámetros 'p_...')
    // Este JSON es más complejo, 512 bytes es un tamaño seguro.
    StaticJsonDocument<512> doc;

    // Rellenamos con los datos hardcodeados del ejemplo de Python
    doc["p_dog_code"] = "LUNA-002";
    doc["p_started_at"] = "2025-10-31T16:57:00Z";
    doc["p_duration_s"] = 7;
    doc["p_result"] = "success";
    doc["p_device_code"] = "ESP32-001"; // ID del dispositivo
    doc["p_co_trainer_id"] = nullptr; // Así se pone un 'null'

    // Crear el objeto anidado "p_conditions"
    JsonObject conditions = doc.createNestedObject("p_conditions");
    conditions["temp"] = 20.0;
    conditions["wind"] = "SO 1km/h";

    // Crear el objeto anidado "p_type"
    JsonObject typ = doc.createNestedObject("p_type");
    typ["scent"] = "Explosivos";
    typ["mode"] = "inicial";

    // Serializar el JSON a un String
    String jsonBody;
    serializeJson(doc, jsonBody);

    Serial.println("Sending JSON to RPC:");
    Serial.println(jsonBody);

    // 3. Configurar la petición HTTP
    HTTPClient http;
    http.begin(url);
    http.addHeader("apikey", SUPABASE_API_KEY);
    http.addHeader("Authorization", String("Bearer ") + accessToken);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Prefer", "return=representation");

    // 4. Enviar la petición POST
    int httpCode = http.POST(jsonBody);

    // 5. Procesar la respuesta
    if (httpCode == 200) { // RPC exitosa devuelve 200 OK
        Serial.println("RPC call successful! (HTTP 200)");
        Serial.println("--- RPC Response (Pretty JSON) ---");
        String payload = http.getString();
        
        // Usamos un DynamicDoc para la respuesta, no sabemos el tamaño
        DynamicJsonDocument responseDoc(1024);
        DeserializationError error = deserializeJson(responseDoc, payload);
        if (error) {
            Serial.print("Failed to parse response JSON. Error: ");
            Serial.println(error.c_str());
        } else {
            serializeJsonPretty(responseDoc, Serial);
            Serial.println();
        }
        Serial.println("----------------------------------");
    } else {
        Serial.printf("[HTTP] Error on RPC POST: %d\n", httpCode);
        String payload = http.getString();
        Serial.println("Error response: " + payload);
    }

    http.end();
    return httpCode;
}

// --- NUEVA FUNCIÓN: Record Batch (RPC) ---
int recordTrainingBatch(String accessToken) {

    // 1. Construir la URL de la RPC
    String url = String(SUPABASE_URL) + "/rest/v1/rpc/record_training_batch";

    // 2. Construir el JSON del Payload
    // Usamos 1024B, que debería ser suficiente para las 2 sesiones de ejemplo.
    StaticJsonDocument<1024> doc;

    // El payload final es un OBJETO que CONTIENE un ARRAY
    // json={"p_items": payload}
    JsonArray p_items = doc.createNestedArray("p_items");

    // --- Sesión 1 (del ejemplo de Python) ---
    JsonObject session1 = p_items.createNestedObject();
    session1["p_dog_code"] = "NEWT-001";
    session1["p_started_at"] = "2025-10-31T17:12:00Z";
    session1["p_duration_s"] = 31;
    session1["p_result"] = "success";
    session1["p_device_code"] = "ESP32-001";
    // Objeto anidado "p_conditions"
    JsonObject cond1 = session1.createNestedObject("p_conditions");
    cond1["temp"] = 17.1;
    // Objeto anidado "p_type"
    JsonObject type1 = session1.createNestedObject("p_type");
    type1["scent"] = "Explosivos";
    type1["mode"] = "basico";

    // --- Sesión 2 (del ejemplo de Python) ---
    JsonObject session2 = p_items.createNestedObject();
    session2["p_dog_code"] = "SIMON-01";
    session2["p_started_at"] = "2025-10-31T17:35:00Z";
    session2["p_duration_s"] = 10;
    session2["p_result"] = "fail";
    session2["p_device_code"] = "ESP32-001";
    session2["p_co_trainer_id"] = nullptr; // Así se pone 'None'
    // Objeto anidado "p_conditions"
    JsonObject cond2 = session2.createNestedObject("p_conditions");
    cond2["temp"] = 25.0;
    // Objeto anidado "p_type"
    JsonObject type2 = session2.createNestedObject("p_type");
    type2["scent"] = "Narcoticos";
    type2["mode"] = "ambiental";

    // Serializar el JSON a un String
    String jsonBody;
    serializeJson(doc, jsonBody);

    Serial.println("Sending JSON to RPC Batch:");
    Serial.println(jsonBody);

    // 3. Configurar la petición HTTP
    HTTPClient http;
    http.begin(url);
    http.addHeader("apikey", SUPABASE_API_KEY);
    http.addHeader("Authorization", String("Bearer ") + accessToken);
    http.addHeader("Content-Type", "application/json");

    // 4. Enviar la petición POST
    int httpCode = http.POST(jsonBody);

    // 5. Procesar la respuesta
    // Para una RPC de batch, 200 (OK) o 204 (No Content) con éxito
    if (httpCode == 200 || httpCode == 204) {
        Serial.printf("RPC Batch call successful! (HTTP %d)\n", httpCode);
        String payload = http.getString(); // Puede estar vacío si es 204
        if (payload.length() > 0) {
            Serial.println("Response: " + payload);
        }
    } else {
        Serial.printf("[HTTP] Error on RPC Batch POST: %d\n", httpCode);
        String payload = http.getString();
        Serial.println("Error response: " + payload);
    }

    http.end();
    return httpCode;
}
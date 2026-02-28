/****************************************************************
 * @file TrainingSession.cpp
 * @brief Implements the TrainingSession data model class.
 ****************************************************************/

/****************************************************************
 * Headers
 ****************************************************************/
#include "TrainingSession.h"
#include <time.h>
#include "config.h"

/****************************************************************
 * Class Method Implementations
 ****************************************************************/

TrainingSession::TrainingSession() 
    : p_duration_s(0), p_result("unknown"), p_timeout_s(0){

}

// --- Setters (ConfigState) ---
void TrainingSession::setDogCode(String code)           { p_dog_code = code; }
void TrainingSession::setStartedAt(String isoTime)      { p_started_at = isoTime; }
void TrainingSession::setConditions(String jsonString)  { p_conditions_json = jsonString; }
void TrainingSession::setType(String jsonString)        { p_type_json = jsonString; }
void TrainingSession::setDeviceCode(String code)        { p_device_code = code; }
void TrainingSession::setTimeout(int seconds)           { p_timeout_s = seconds; }

// --- Setters (PlayState) ---
void TrainingSession::setDuration(int seconds) { p_duration_s = seconds; }
void TrainingSession::setResult(String result) { p_result = result; }

// --- Getters ---
int TrainingSession::getDuration() { return p_duration_s; }
int TrainingSession::getTimeout() { return p_timeout_s; }

// --- Timers ---
void TrainingSession::addSecondsToTimeStamp(int seconds) {
    if (p_started_at.length() == 0) return;

    struct tm tm_time = {0};
    int ms = 0;
    
    // 1. Extraemos los números del texto "YYYY-MM-DDTHH:MM:SS.SSSZ"
    int parsed = sscanf(p_started_at.c_str(), "%d-%d-%dT%d:%d:%d.%dZ", 
           &tm_time.tm_year, &tm_time.tm_mon, &tm_time.tm_mday, 
           &tm_time.tm_hour, &tm_time.tm_min, &tm_time.tm_sec, &ms);
           
    if (parsed >= 6) { 
        // 2. Ajustamos los valores al estándar de C++
        tm_time.tm_year -= 1900;
        tm_time.tm_mon -= 1;
        
        // 3. ¡LA MAGIA! Le sumamos los segundos que pasaron
        tm_time.tm_sec += seconds;
        
        // mktime() es inteligente: si los segundos se pasan de 60, suma 1 minuto; 
        // si los minutos pasan de 60, suma 1 hora, etc. Normaliza todo solo.
        mktime(&tm_time); 
        
        // 4. Volvemos a armar el string limpio
        char buf[32];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
                 tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
                 tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec, ms);
                 
        // Guardamos la nueva hora para la próxima ronda
        p_started_at = String(buf);
    }
}


bool TrainingSession::serialize(String &outJsonString) {
    StaticJsonDocument<512> doc;    // 512 bytes for one session 
    
    // 1. Asignar valores simples
    doc["p_dog_code"] = p_dog_code;
    doc["p_started_at"] = p_started_at;
    doc["p_duration_s"] = p_duration_s;
    doc["p_result"] = p_result;
    doc["p_device_code"] = p_device_code;
    doc["p_co_trainer_id"] = nullptr; // O agregar p_co_trainer_id

    // 2. Manejar JSON anidado (Conditions)
    if (p_conditions_json.length() > 0) {
        // Necesitamos un Doc temporal para parsear el string
        StaticJsonDocument<128> tempDoc;
        DeserializationError err = deserializeJson(tempDoc, p_conditions_json);
        if (err) {
            LOG_PRINTLN("Error parsing p_conditions_json");
        } else {
            doc["p_conditions"] = tempDoc.as<JsonObject>();
        }
    }

    // 3. Manejar JSON anidado (Type)
    if (p_type_json.length() > 0) {
        StaticJsonDocument<128> tempDoc;
        DeserializationError err = deserializeJson(tempDoc, p_type_json);
        if (err) {
            LOG_PRINTLN("Error parsing p_type_json");
        } else {
            doc["p_type"] = tempDoc.as<JsonObject>();
        }
    }

    // 4. Serializar el documento completo
    if (serializeJson(doc, outJsonString) == 0) {
        LOG_PRINTLN("Failed to serialize TrainingSession");
        return false;
    }
    
    return true;
}
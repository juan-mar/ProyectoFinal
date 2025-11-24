/****************************************************************
 * @file TrainingSession.cpp
 * @brief Implements the TrainingSession data model class.
 ****************************************************************/

/****************************************************************
 * Headers
 ****************************************************************/
#include "TrainingSession.h"
#include "config.h"

/****************************************************************
 * Class Method Implementations
 ****************************************************************/

TrainingSession::TrainingSession() 
    : p_duration_s(0), p_result("unknown"){

}

// --- Setters (ConfigState) ---
void TrainingSession::setDogCode(String code)           { p_dog_code = code; }
void TrainingSession::setStartedAt(String isoTime)      { p_started_at = isoTime; }
void TrainingSession::setConditions(String jsonString)  { p_conditions_json = jsonString; }
void TrainingSession::setType(String jsonString)        { p_type_json = jsonString; }
void TrainingSession::setDeviceCode(String code)        { p_device_code = code; }

// --- Setters (PlayState) ---
void TrainingSession::setDuration(int seconds) { p_duration_s = seconds; }
void TrainingSession::setResult(String result) { p_result = result; }


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
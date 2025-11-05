/****************************************************************
 * @file TrainingSession.h
 * @brief Declares the TrainingSession data model class (entity).
 ****************************************************************/

/****************************************************************
 * Include Guards
 ****************************************************************/
#ifndef TRAINING_SESSION_H
#define TRAINING_SESSION_H

/****************************************************************
 * Headers
 ****************************************************************/
#include <Arduino.h>
#include <ArduinoJson.h> // We need this for serialization

/****************************************************************
 * Class Declarations
 ****************************************************************/

/**
 * @brief A data model class (entity) that holds the data
 * for a single training session. It is created in ConfigState,
 * passed to a PlayState, and then given to DataManager to save.
 */
class TrainingSession {
public:
    /**
     * @brief Constructor vacío, como solicitaste.
     */
    TrainingSession();

    // --- Setters (Usados por ConfigState) ---
    void setDogCode(String code);
    void setStartedAt(String isoTime);
    void setConditions(String jsonString); // e.g., "{\"temp\":20}"
    void setType(String jsonString);       // e.g., "{\"mode\":\"auto\"}"
    void setDeviceCode(String code);

    // --- Setters (Usados por los estados de Play) ---
    void setDuration(int seconds);
    void setResult(String result); // e.g., "success", "fail"

    /**
     * @brief Serializes the entire session object into a JSON string,
     * ready to be saved to the log or sent to Supabase.
     * @param outJsonString The String object to write the output to.
     * @return true if serialization was successful.
     */
    bool serialize(String &outJsonString);

private:
    // Almacenamos todos los parámetros de la RPC
    String p_dog_code;
    String p_started_at;
    int    p_duration_s;
    String p_result;
    String p_conditions_json; // Almacenamos como strings
    String p_type_json;       // Almacenamos como strings
    String p_device_code;
    String p_co_trainer_id; // (Podemos añadir un setter si es necesario)
};

#endif // TRAINING_SESSION_H
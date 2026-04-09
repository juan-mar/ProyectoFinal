#ifndef ENVIRONMENT_SENSOR_H
#define ENVIRONMENT_SENSOR_H

#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>

struct EnvData {
    float temperature;
    float humidity;
    float pressure;
    bool valid;
};

class EnvironmentSensor {
private:
    Adafruit_BME280 _bme;
    bool _initialized = false;
    
    // Caché de último valor válido
    EnvData _lastValidEnvData = {0, 0, 0, false};
    unsigned long _lastValidReadTime = 0;

public:
    bool init();
    
    /**
     * @brief Realiza lectura del sensor y actualiza caché si es válida.
     * Esta función debe llamarse periódicamente (~1 minuto).
     */
    void updateReadings();
    
    /**
     * @brief Obtiene el último valor válido del sensor (desde caché RAM).
     * @return EnvData con temperature, humidity, pressure y valid flag.
     */
    EnvData getLastValidReadings();
};

/*
marron sda
verde scl
negro gnd 
rojo vin  

*/

#endif
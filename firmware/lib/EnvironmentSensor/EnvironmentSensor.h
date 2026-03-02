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

public:
    bool init();
    EnvData getReadings();
};

#endif
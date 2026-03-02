#include "EnvironmentSensor.h"
#include "Config.h"
#include "HardwareConfig.h"

bool EnvironmentSensor::init() {
    // 0x76 o 0x77 son las direcciones comunes del BME280
    if (!_bme.begin(BME280_ADDRESS)) {
        return false;
    }
    _initialized = true;
    return true;
}

EnvData EnvironmentSensor::getReadings() {
    EnvData data = {0, 0, 0, false};
    if (!_initialized) return data;

    data.temperature = _bme.readTemperature();
    data.humidity = _bme.readHumidity();
    data.pressure = _bme.readPressure() / 100.0F; // hPa
    data.valid = true;
    return data;
}
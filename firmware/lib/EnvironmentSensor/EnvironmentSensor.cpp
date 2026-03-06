#include "EnvironmentSensor.h"
#include "Config.h"
#include "HardwareConfig.h"

bool EnvironmentSensor::init() {
    // 0x76 o 0x77 son las direcciones comunes del BME280
    if (!_bme.begin(ENV_BME280_I2C_ADDRESS)) {
        return false;
    }
    _initialized = true;
    _lastValidReadTime = 0;
    return true;
}

void EnvironmentSensor::updateReadings() {
    if (!_initialized) return;
    
    unsigned long now = millis();
    
    // Intentar leer del sensor
    float temp = _bme.readTemperature();
    float humidity = _bme.readHumidity();
    float pressure = _bme.readPressure() / 100.0F; // hPa
    
    // Validar que los valores sean razonables (sensatez adicional)
    // BME280: -40°C a +85°C, 0-100% humidity, 300-1100 hPa
    if (temp > -40 && temp < 85 && humidity >= 0 && humidity <= 100 && pressure >= 300 && pressure <= 1100) {
        _lastValidEnvData.temperature = temp;
        _lastValidEnvData.humidity = humidity;
        _lastValidEnvData.pressure = pressure;
        _lastValidEnvData.valid = true;
        _lastValidReadTime = now;
        
        LOG_PRINTF("[EnvSensor] Valid reading: T=%.1f°C, H=%.1f%%, P=%.1f hPa (valid at %lu ms)\n", 
                   temp, humidity, pressure, _lastValidReadTime);
    } else {
        LOG_PRINTF("[EnvSensor] Invalid reading: T=%.1f, H=%.1f, P=%.1f - Using cached value\n", 
                   temp, humidity, pressure);
    }
}

EnvData EnvironmentSensor::getLastValidReadings() {
    return _lastValidEnvData;
}
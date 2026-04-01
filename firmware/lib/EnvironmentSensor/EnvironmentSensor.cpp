#include "EnvironmentSensor.h"
#include "Config.h"
#include "HardwareConfig.h"
#include <Wire.h>

bool EnvironmentSensor::init() {
    LOG_PRINTF("[EnvSensor] Init: SDA=%d SCL=%d prefAddr=0x%02X\n",
               PIN_BME_SDA,
               PIN_BME_SCL,
               ENV_BME280_I2C_ADDRESS);

    Wire.begin(PIN_BME_SDA, PIN_BME_SCL);

    // Try preferred address first, then fallback to the other common BME280 address.
    uint8_t firstAddr = static_cast<uint8_t>(ENV_BME280_I2C_ADDRESS);
    uint8_t secondAddr = (firstAddr == 0x76) ? 0x77 : 0x76;

    if (!_bme.begin(firstAddr, &Wire)) {
        LOG_PRINTF("[EnvSensor] BME280 not found at 0x%02X, trying 0x%02X...\n",
                   firstAddr,
                   secondAddr);

        if (!_bme.begin(secondAddr, &Wire)) {
            LOG_PRINTLN("[EnvSensor] ERROR: BME280 init failed on 0x76 and 0x77.");
            return false;
        }

        LOG_PRINTF("[EnvSensor] BME280 initialized at fallback address 0x%02X\n", secondAddr);
    } else {
        LOG_PRINTF("[EnvSensor] BME280 initialized at preferred address 0x%02X\n", firstAddr);
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
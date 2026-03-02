/****************************************************************
 * @file BatteryMonitor.cpp
 * @brief Implements the BatteryMonitor class methods.
 ****************************************************************/

/****************************************************************
 * Headers
 ****************************************************************/
#include "BatteryMonitor.h"
#include "Config.h" // For logging macros

/****************************************************************
 * Defines and Constants
 ****************************************************************/
// ADC Configuration
#define ADC_RESOLUTION 4095.0       ///< ESP32 ADC 12-bit resolution
#define ADC_REFERENCE_VOLTAGE 3.3   ///< ESP32 ADC reference voltage in volts

// EMA Filter Configuration (Exponential Moving Average)
#define EMA_ALPHA 0.2               ///< EMA smoothing factor (0.0-1.0)
                                    ///< Lower = more smoothing, Higher = more responsive

// LiPo Battery Voltage Thresholds
#define LIPO_VOLTAGE_MAX 4.2        ///< Maximum LiPo voltage (fully charged)
#define LIPO_VOLTAGE_MIN 3.4        ///< Minimum LiPo voltage (safe discharge limit)
#define LIPO_CRITICAL_PERCENT 10    ///< Critical battery percentage threshold

/****************************************************************
 * Class Method Implementations
 ****************************************************************/

BatteryMonitor::BatteryMonitor(uint8_t pin, float multiplier) 
    : _pin(pin), _multiplier(multiplier), _initialized(false), 
      _filteredVoltage(0.0), _alpha(EMA_ALPHA)
{
    LOG_PRINTF("BatteryMonitor: Constructor called (pin=%d, multiplier=%.2f)\n", pin, multiplier);
}

bool BatteryMonitor::begin() {
    LOG_PRINTLN("BatteryMonitor: Initializing...");
    
    // Configure ADC pin as input
    pinMode(_pin, INPUT);
    
    // Set ADC attenuation to 0dB (range 0-1.1V for max precision)
    analogSetPinAttenuation(_pin, ADC_0db);
    
    // Initialize EMA filter with first reading
    uint16_t rawValue = analogRead(_pin);
    _filteredVoltage = (rawValue / ADC_RESOLUTION) * ADC_REFERENCE_VOLTAGE * _multiplier;
    
    // Verify pin configuration
    if (rawValue == 0) {
        LOG_PRINTLN("BatteryMonitor: Warning - Initial reading is 0, check hardware connection.");
    }
    
    _initialized = true;
    LOG_PRINTF("BatteryMonitor: Initialization complete (initial voltage=%.2fV).\n", _filteredVoltage);
    return true;
}

BatteryInfo BatteryMonitor::getInfo() {
    BatteryInfo info;
    
    if (!_initialized) {
        LOG_PRINTLN("BatteryMonitor: ERROR - Not initialized!");
        info.voltage = 0.0;
        info.percentage = 0;
        info.isCritical = true;
        return info;
    }
    
    // Read voltage with oversampling
    info.voltage = getVoltage();
    
    // Calculate percentage based on LiPo voltage range (3.4V to 4.2V)
    // Formula: ((V - Vmin) / (Vmax - Vmin)) * 100
    info.percentage = (int)((info.voltage - LIPO_VOLTAGE_MIN) * 100.0 / 
                             (LIPO_VOLTAGE_MAX - LIPO_VOLTAGE_MIN));
    
    // Apply constraints to keep percentage within 0-100 range
    if (info.percentage > 100) {
        info.percentage = 100;
    }
    if (info.percentage < 0) {
        info.percentage = 0;
    }
    
    // Determine critical status
    info.isCritical = (info.percentage <= LIPO_CRITICAL_PERCENT);
    
    // Log battery status
    LOG_PRINTF("BatteryMonitor: Voltage=%.2fV, Percentage=%d%%, Critical=%s\n", 
               info.voltage, info.percentage, info.isCritical ? "YES" : "NO");
    
    // Warning for critical battery
    if (info.isCritical) {
        LOG_PRINTLN("BatteryMonitor: WARNING - Battery level is CRITICAL!");
    }
    
    return info;
}

float BatteryMonitor::getVoltage() {
    if (!_initialized) {
        LOG_PRINTLN("BatteryMonitor: ERROR - Not initialized!");
        return 0.0;
    }
    
    // Read current ADC value (single reading - non-blocking)
    uint16_t rawValue = analogRead(_pin);
    
    // Convert ADC reading to voltage
    // Formula: (ADC_Reading / ADC_Resolution) * Vref * Multiplier
    float currentVoltage = (rawValue / ADC_RESOLUTION) * ADC_REFERENCE_VOLTAGE * _multiplier;
    
    // Apply EMA filter: filtered_n = alpha * current + (1 - alpha) * filtered_(n-1)
    // This provides smooth readings without blocking the task
    _filteredVoltage = (_alpha * currentVoltage) + ((1.0 - _alpha) * _filteredVoltage);
    
    return _filteredVoltage;
}

bool BatteryMonitor::isInitialized() const {
    return _initialized;
}
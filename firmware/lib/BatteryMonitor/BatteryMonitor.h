/****************************************************************
 * @file BatteryMonitor.h
 * @brief Declares the BatteryMonitor class for battery voltage
 * monitoring and percentage calculation.
 ****************************************************************/

/****************************************************************
 * Include Guards
 ****************************************************************/
#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

/****************************************************************
 * Headers
 ****************************************************************/
#include <Arduino.h>

/****************************************************************
 * Structures
 ****************************************************************/

/**
 * @brief Battery information structure.
 * Contains voltage, percentage, and critical status.
 */
struct BatteryInfo {
    float voltage;       ///< Battery voltage in volts
    int percentage;      ///< Battery percentage (0-100)
    bool isCritical;     ///< True if battery is below critical threshold
};

/****************************************************************
 * Class Declarations
 ****************************************************************/

/**
 * @brief Monitors battery voltage and calculates battery percentage.
 * This class handles ADC reading with oversampling for noise reduction
 * and provides battery status information.
 */
class BatteryMonitor {
private:
    uint8_t _pin;           ///< ADC pin for battery voltage reading
    float _multiplier;      ///< Voltage multiplier for voltage divider
    bool _initialized;      ///< Initialization status flag
    float _filteredVoltage; ///< EMA filtered voltage value
    float _alpha;           ///< EMA smoothing factor (0.0-1.0)

public:
    /**
     * @brief Constructor.
     * @param pin The ADC pin connected to the battery voltage divider.
     * @param multiplier The voltage multiplier based on the divider ratio.
     *                   Default is 2.0 for a 1:1 voltage divider.
     */
    BatteryMonitor(uint8_t pin, float multiplier = 2.0);
    
    /**
     * @brief Initializes the battery monitor.
     * Configures the ADC pin for reading.
     * @return true if initialization was successful, false otherwise.
     */
    bool begin();
    
    /**
     * @brief Reads battery voltage and calculates battery information.
     * Uses EMA (Exponential Moving Average) filter to reduce electrical noise.
     * This method is non-blocking and suitable for FreeRTOS tasks.
     * @return BatteryInfo structure with voltage, percentage, and status.
     */
    BatteryInfo getInfo();
    
    /**
     * @brief Gets the raw battery voltage without percentage calculation.
     * @return The battery voltage in volts.
     */
    float getVoltage();
    
    /**
     * @brief Checks if the battery monitor has been initialized.
     * @return true if initialized, false otherwise.
     */
    bool isInitialized() const;
};

#endif // BATTERY_MONITOR_H
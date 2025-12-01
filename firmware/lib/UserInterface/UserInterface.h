/****************************************************************
 * @file UserInterface.h
 * @brief Handles physical user interaction: Button inputs,
 * LED feedback patterns, and Battery monitoring.
 ****************************************************************/
#ifndef USER_INTERFACE_H
#define USER_INTERFACE_H

#include <Arduino.h>
#include <freertos/queue.h>
#include "Events.h"

// --- Pin Definitions ---
#define PIN_MODE_SWITCH  25
#define PIN_LED_RED      2
#define PIN_LED_GREEN    4
#define PIN_LED_BLUE     16
#define PIN_BATTERY_ADC  34
#define PIN_PWR_REMOTE_RX 14

// --- Visual Feedback States ---
enum LedPattern {
    LED_OFF,
    LED_IDLE_OFFLINE, // Green slow blink
    LED_IDLE_ONLINE,  // Blue slow blink
    LED_SYNCING,      // Blue fast blink
    LED_SUCCESS,      // Green solid
    LED_ERROR_WIFI,   // Red solid
    LED_ERROR_DB,     // Red fast blink
    LED_LOW_BATTERY   // Yellow slow blink
};

class UserInterface {
public:
    UserInterface();

    /**
     * @brief Initializes GPIOs, Interrupts and ADC.
     * @param fsmQueue Handle to the FSM event queue.
     */
    void init(QueueHandle_t fsmQueue);

    /**
     * @brief Updates LED blinking patterns and reads battery.
     * Call this in the main loop (non-blocking).
     */
    void update();

    void setLedPattern(LedPattern pattern);

    void disableSwitchInterrupt();
    void enableSwitchInterrupt();

    int getBatteryPercentage();
    void setRemoteRxPower(bool enable);

private:
    static QueueHandle_t fsmQueue; // Static for ISR access

    // LED Logic
    LedPattern currentPattern;
    unsigned long lastBlinkTime;
    bool ledStateHigh; 

    // Battery Logic
    unsigned long lastBatteryCheck;
    int currentBatteryLevel;

    // Helpers
    void updateLeds();
    void setRgbColor(bool r, bool g, bool b);
    void readBattery();

    // ISR
    static void IRAM_ATTR isrModeSwitch();
};

#endif // USER_INTERFACE_H
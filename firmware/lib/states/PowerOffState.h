/****************************************************************
 * @file PowerOffState.h
 * @brief Declares the PowerOffState class (shutdown/deep sleep).
 ****************************************************************/

#ifndef POWER_OFF_STATE_H
#define POWER_OFF_STATE_H

#include "State.h"

/**
 * @brief Power Off state for shutdown and deep sleep.
 * 
 * This state is entered when:
 * - USB is connected (charging started)
 * - Power switch is turned OFF
 * 
 * In this state:
 * - All peripherals are powered off (WiFi, Remoto, BME280, LEDs, Launcher, Solenoid)
 * - CPU enters deep sleep (~10µA power consumption)
 * 
 * Wakeup sources:
 * - Power switch turned back ON (GPIO 14, rising edge only)
 * - On wakeup, ESP32 performs a complete reset and runs setup() again
 */
class PowerOffState : public State {
public:
    PowerOffState();

    virtual void enter(StateManager* manager) override;
    virtual void execute(StateManager* manager) override;
    virtual void exit(StateManager* manager) override;

protected:
    virtual void handleEvent(StateManager* manager, Event& event) override;
    virtual void update(StateManager* manager) override;
};

#endif // POWER_OFF_STATE_H

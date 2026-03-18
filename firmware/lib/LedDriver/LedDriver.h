#ifndef LED_DRIVER_H
#define LED_DRIVER_H

#include <Arduino.h>

class LedDriver {
public:
    LedDriver();

    void begin();
    bool isInitialized() const;

    void setGlobalBrightness(uint8_t brightness);
    uint8_t globalBrightness() const;

    void setRawColor(uint8_t red, uint8_t green, uint8_t blue);
    void turnOff();

private:
    uint8_t clampBrightness(uint8_t brightness) const;
    uint32_t scaleColorToDuty(uint8_t colorValue) const;
    uint32_t resolveOutputDuty(uint8_t colorValue) const;
    void applyOutput();

    bool _initialized;
    bool _activeHigh;
    uint8_t _globalBrightness;
    uint8_t _red;
    uint8_t _green;
    uint8_t _blue;
};

#endif // LED_DRIVER_H
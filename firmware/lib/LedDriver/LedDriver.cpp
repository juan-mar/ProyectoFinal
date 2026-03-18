#include "LedDriver.h"
#include "HardwareConfig.h"

LedDriver::LedDriver()
    : _initialized(false), _activeHigh(LED_ACTIVE != 0),
      _globalBrightness(LED_BRIGHTNESS_DEFAULT),
      _red(LED_COLOR_MIN), _green(LED_COLOR_MIN), _blue(LED_COLOR_MIN) {}

void LedDriver::begin() {
#if ENABLE_LED_CONTROL
    ledcSetup(LED_PWM_CHANNEL_R, PWM_FREQUENCY_LED, PWM_BIT_WIDTH_LED);
    ledcSetup(LED_PWM_CHANNEL_G, PWM_FREQUENCY_LED, PWM_BIT_WIDTH_LED);
    ledcSetup(LED_PWM_CHANNEL_B, PWM_FREQUENCY_LED, PWM_BIT_WIDTH_LED);

    ledcAttachPin(PIN_LED_R, LED_PWM_CHANNEL_R);
    ledcAttachPin(PIN_LED_G, LED_PWM_CHANNEL_G);
    ledcAttachPin(PIN_LED_B, LED_PWM_CHANNEL_B);

    _initialized = true;
    turnOff();
#else
    _initialized = false;
#endif
}

bool LedDriver::isInitialized() const {
    return _initialized;
}

void LedDriver::setGlobalBrightness(uint8_t brightness) {
    _globalBrightness = clampBrightness(brightness);
    applyOutput();
}

uint8_t LedDriver::globalBrightness() const {
    return _globalBrightness;
}

void LedDriver::setRawColor(uint8_t red, uint8_t green, uint8_t blue) {
    _red = red;
    _green = green;
    _blue = blue;
    applyOutput();
}

void LedDriver::turnOff() {
    _red = LED_COLOR_MIN;
    _green = LED_COLOR_MIN;
    _blue = LED_COLOR_MIN;
    applyOutput();
}

uint8_t LedDriver::clampBrightness(uint8_t brightness) const {
    if (brightness < LED_BRIGHTNESS_MIN) {
        return LED_BRIGHTNESS_MIN;
    }
    if (brightness > LED_BRIGHTNESS_MAX) {
        return LED_BRIGHTNESS_MAX;
    }
    return brightness;
}

uint32_t LedDriver::scaleColorToDuty(uint8_t colorValue) const {
    uint32_t scaledByBrightness =
        (static_cast<uint32_t>(colorValue) * static_cast<uint32_t>(_globalBrightness)) /
        static_cast<uint32_t>(LED_BRIGHTNESS_MAX);

    return (scaledByBrightness * static_cast<uint32_t>(LED_PWM_MAX_DUTY)) /
           static_cast<uint32_t>(LED_COLOR_MAX);
}

uint32_t LedDriver::resolveOutputDuty(uint8_t colorValue) const {
    uint32_t duty = scaleColorToDuty(colorValue);
    return _activeHigh ? duty : (static_cast<uint32_t>(LED_PWM_MAX_DUTY) - duty);
}

void LedDriver::applyOutput() {
#if ENABLE_LED_CONTROL
    if (!_initialized) {
        return;
    }

    ledcWrite(LED_PWM_CHANNEL_R, resolveOutputDuty(_red));
    ledcWrite(LED_PWM_CHANNEL_G, resolveOutputDuty(_green));
    ledcWrite(LED_PWM_CHANNEL_B, resolveOutputDuty(_blue));
#endif
}
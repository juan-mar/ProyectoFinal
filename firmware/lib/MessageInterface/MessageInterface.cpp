#include "MessageInterface.h"
#include "HardwareConfig.h"

MessageInterface::MessageInterface()
    : _initialized(false), _globalBrightness(MSG_INTERFACE_GLOBAL_BRIGHTNESS_DEFAULT),
      _currentMessage(USER_MSG_OFF), _currentPattern({false, false, false, LED_BRIGHTNESS_MIN, false, 0}),
      _blinkOnState(false), _lastToggleMs(0) {}

void MessageInterface::begin() {
    _ledDriver.begin();
    _initialized = _ledDriver.isInitialized();
    _ledDriver.setGlobalBrightness(_globalBrightness);

    setMessage(USER_MSG_OFF);
}

void MessageInterface::update() {
    if (!_initialized) {
        return;
    }

    if (!_currentPattern.blinking) {
        return;
    }

    uint32_t now = millis();
    if (now - _lastToggleMs >= _currentPattern.blinkRateMs) {
        _blinkOnState = !_blinkOnState;
        applyPatternState(_blinkOnState);
        _lastToggleMs = now;
    }
}

void MessageInterface::setMessage(UserMessage msg) {
    _currentMessage = msg;
    _currentPattern = resolvePattern(msg);
    _lastToggleMs = millis();

    if (_currentPattern.blinking) {
        _blinkOnState = true;
        applyPatternState(true);
    } else {
        _blinkOnState = false;
        applyPatternState(true);
    }
}

UserMessage MessageInterface::currentMessage() const {
    return _currentMessage;
}

void MessageInterface::setGlobalBrightness(uint8_t brightness) {
    _globalBrightness = brightness;
    _ledDriver.setGlobalBrightness(_globalBrightness);

    if (_currentPattern.blinking) {
        applyPatternState(_blinkOnState);
    } else {
        applyPatternState(true);
    }
}

uint8_t MessageInterface::globalBrightness() const {
    return _globalBrightness;
}

void MessageInterface::setRawColor(bool redOn, bool greenOn, bool blueOn) {
    setRawColor(redOn, greenOn, blueOn, MSG_RAW_COLOR_INTENSITY_DEFAULT);
}

void MessageInterface::setRawColor(bool redOn, bool greenOn, bool blueOn, uint8_t intensity) {
    writeColor(redOn, greenOn, blueOn, intensity);
}

MessageInterface::Pattern MessageInterface::resolvePattern(UserMessage msg) const {
    switch (msg) {
        case USER_MSG_IDLE:
            return {MSG_IDLE_R, MSG_IDLE_G, MSG_IDLE_B, MSG_IDLE_INTENSITY, MSG_IDLE_BLINKING, MSG_IDLE_BLINK_RATE_MS};
        case USER_MSG_CALIBRATING:
            return {MSG_CALIBRATING_R, MSG_CALIBRATING_G, MSG_CALIBRATING_B, MSG_CALIBRATING_INTENSITY, MSG_CALIBRATING_BLINKING, MSG_CALIBRATING_BLINK_RATE_MS};
        case USER_MSG_ACTIVE:
            return {MSG_ACTIVE_R, MSG_ACTIVE_G, MSG_ACTIVE_B, MSG_ACTIVE_INTENSITY, MSG_ACTIVE_BLINKING, MSG_ACTIVE_BLINK_RATE_MS};
        case USER_MSG_ERROR:
            return {MSG_ERROR_R, MSG_ERROR_G, MSG_ERROR_B, MSG_ERROR_INTENSITY, MSG_ERROR_BLINKING, MSG_ERROR_BLINK_RATE_MS};
        case USER_MSG_SUCCESS:
            return {MSG_SUCCESS_R, MSG_SUCCESS_G, MSG_SUCCESS_B, MSG_SUCCESS_INTENSITY, MSG_SUCCESS_BLINKING, MSG_SUCCESS_BLINK_RATE_MS};
        case USER_MSG_SYNCING:
            return {MSG_SYNCING_R, MSG_SYNCING_G, MSG_SYNCING_B, MSG_SYNCING_INTENSITY, MSG_SYNCING_BLINKING, MSG_SYNCING_BLINK_RATE_MS};
        case USER_MSG_OFF:
        default:
            return {MSG_OFF_R, MSG_OFF_G, MSG_OFF_B, MSG_OFF_INTENSITY, MSG_OFF_BLINKING, MSG_OFF_BLINK_RATE_MS};
    }
}

void MessageInterface::applyPatternState(bool onState) {
    if (!onState) {
        _ledDriver.turnOff();
        return;
    }

    writeColor(_currentPattern.red, _currentPattern.green, _currentPattern.blue, _currentPattern.intensity);
}

void MessageInterface::writeColor(bool redOn, bool greenOn, bool blueOn, uint8_t intensity) {
    if (!_initialized) {
        return;
    }

    uint8_t redValue = redOn ? intensity : LED_COLOR_MIN;
    uint8_t greenValue = greenOn ? intensity : LED_COLOR_MIN;
    uint8_t blueValue = blueOn ? intensity : LED_COLOR_MIN;
    _ledDriver.setRawColor(redValue, greenValue, blueValue);
}

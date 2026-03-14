#include "MessageInterface.h"
#include "HardwareConfig.h"

MessageInterface::MessageInterface()
    : _activeHigh(LED_ACTIVE), _initialized(false),
      _currentMessage(USER_MSG_OFF), _currentPattern({false, false, false, false, 0}),
      _blinkOnState(false), _lastToggleMs(0) {}

void MessageInterface::begin() {
#if ENABLE_LED_CONTROL
    pinMode(PIN_LED_R, OUTPUT);
    pinMode(PIN_LED_G, OUTPUT);
    pinMode(PIN_LED_B, OUTPUT);
    _initialized = true;
#else
    _initialized = false;
#endif

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

void MessageInterface::setRawColor(bool redOn, bool greenOn, bool blueOn) {
    writeColor(redOn, greenOn, blueOn);
}

MessageInterface::Pattern MessageInterface::resolvePattern(UserMessage msg) const {
    switch (msg) {
        case USER_MSG_IDLE:
            return {MSG_IDLE_R, MSG_IDLE_G, MSG_IDLE_B, MSG_IDLE_BLINKING, MSG_IDLE_BLINK_RATE_MS};
        case USER_MSG_CALIBRATING:
            return {MSG_CALIBRATING_R, MSG_CALIBRATING_G, MSG_CALIBRATING_B, MSG_CALIBRATING_BLINKING, MSG_CALIBRATING_BLINK_RATE_MS};
        case USER_MSG_ACTIVE:
            return {MSG_ACTIVE_R, MSG_ACTIVE_G, MSG_ACTIVE_B, MSG_ACTIVE_BLINKING, MSG_ACTIVE_BLINK_RATE_MS};
        case USER_MSG_ERROR:
            return {MSG_ERROR_R, MSG_ERROR_G, MSG_ERROR_B, MSG_ERROR_BLINKING, MSG_ERROR_BLINK_RATE_MS};
        case USER_MSG_SUCCESS:
            return {MSG_SUCCESS_R, MSG_SUCCESS_G, MSG_SUCCESS_B, MSG_SUCCESS_BLINKING, MSG_SUCCESS_BLINK_RATE_MS};
        case USER_MSG_SYNCING:
            return {MSG_SYNCING_R, MSG_SYNCING_G, MSG_SYNCING_B, MSG_SYNCING_BLINKING, MSG_SYNCING_BLINK_RATE_MS};
        case USER_MSG_OFF:
        default:
            return {MSG_OFF_R, MSG_OFF_G, MSG_OFF_B, MSG_OFF_BLINKING, MSG_OFF_BLINK_RATE_MS};
    }
}

void MessageInterface::applyPatternState(bool onState) {
    if (!onState) {
        writeColor(false, false, false);
        return;
    }

    writeColor(_currentPattern.red, _currentPattern.green, _currentPattern.blue);
}

void MessageInterface::writeColor(bool redOn, bool greenOn, bool blueOn) {
#if ENABLE_LED_CONTROL
    if (!_initialized) {
        return;
    }

    digitalWrite(PIN_LED_R, (_activeHigh ? redOn : !redOn) ? HIGH : LOW);
    digitalWrite(PIN_LED_G, (_activeHigh ? greenOn : !greenOn) ? HIGH : LOW);
    digitalWrite(PIN_LED_B, (_activeHigh ? blueOn : !blueOn) ? HIGH : LOW);
#else
    (void)redOn;
    (void)greenOn;
    (void)blueOn;
#endif
}

#ifndef MESSAGE_INTERFACE_H
#define MESSAGE_INTERFACE_H

#include <Arduino.h>

// --- MessageInterface mapping (RGB backend) ---
// Color channels (0/1), blink enable (0/1), and blink period (ms).
#define MSG_OFF_R 0
#define MSG_OFF_G 0
#define MSG_OFF_B 0
#define MSG_OFF_BLINKING 0
#define MSG_OFF_BLINK_RATE_MS 0

#define MSG_IDLE_R 0
#define MSG_IDLE_G 0
#define MSG_IDLE_B 1
#define MSG_IDLE_BLINKING 1
#define MSG_IDLE_BLINK_RATE_MS 1000

#define MSG_CALIBRATING_R 0
#define MSG_CALIBRATING_G 1
#define MSG_CALIBRATING_B 1
#define MSG_CALIBRATING_BLINKING 1
#define MSG_CALIBRATING_BLINK_RATE_MS 300

#define MSG_ACTIVE_R 1
#define MSG_ACTIVE_G 1
#define MSG_ACTIVE_B 1
#define MSG_ACTIVE_BLINKING 0
#define MSG_ACTIVE_BLINK_RATE_MS 0

#define MSG_ERROR_R 1
#define MSG_ERROR_G 0
#define MSG_ERROR_B 0
#define MSG_ERROR_BLINKING 1
#define MSG_ERROR_BLINK_RATE_MS 150

#define MSG_SUCCESS_R 0
#define MSG_SUCCESS_G 1
#define MSG_SUCCESS_B 0
#define MSG_SUCCESS_BLINKING 1
#define MSG_SUCCESS_BLINK_RATE_MS 200

#define MSG_SYNCING_R 1
#define MSG_SYNCING_G 1
#define MSG_SYNCING_B 0
#define MSG_SYNCING_BLINKING 1
#define MSG_SYNCING_BLINK_RATE_MS 350

/**
 * @brief User-facing messages abstracted from physical output devices.
 *
 * Today this maps to a single RGB LED.
 * Future outputs (OLED/LCD/etc.) can reuse the same messages.
 */
enum UserMessage {
    USER_MSG_OFF = 0,
    USER_MSG_IDLE,
    USER_MSG_CALIBRATING,
    USER_MSG_ACTIVE,
    USER_MSG_ERROR,
    USER_MSG_SUCCESS,
    USER_MSG_SYNCING
};

class MessageInterface {
public:
    MessageInterface();

    void begin();
    void update();

    void setMessage(UserMessage msg);
    UserMessage currentMessage() const;

    // Low-level helper for transitional/debug use-cases.
    void setRawColor(bool redOn, bool greenOn, bool blueOn);

private:
    struct Pattern {
        bool red;
        bool green;
        bool blue;
        bool blinking;
        uint32_t blinkRateMs;
    };

    Pattern resolvePattern(UserMessage msg) const;
    void applyPatternState(bool onState);
    void writeColor(bool redOn, bool greenOn, bool blueOn);

    bool _activeHigh;
    bool _initialized;

    UserMessage _currentMessage;
    Pattern _currentPattern;
    bool _blinkOnState;
    uint32_t _lastToggleMs;
};

#endif // MESSAGE_INTERFACE_H

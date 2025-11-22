/****************************************************************
 * @file UserInterface.cpp
 * @brief Implements UserInterface methods.
 ****************************************************************/

#include "UserInterface.h"
#include "config.h"

// Initialize static member
QueueHandle_t UserInterface::fsmQueue = nullptr;

// Timing
#define BLINK_SLOW_MS 1000
#define BLINK_FAST_MS 200
#define BATTERY_CHECK_MS 60000

UserInterface::UserInterface() 
    : currentPattern(LED_OFF), lastBlinkTime(0), ledStateHigh(false),
      lastBatteryCheck(0), currentBatteryLevel(100){
}

void UserInterface::init(QueueHandle_t queue) {
    LOG_PRINTLN("UI: Initializing hardware...");
    fsmQueue = queue;

    pinMode(PIN_MODE_SWITCH, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_MODE_SWITCH), isrModeSwitch, CHANGE);

    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_BLUE, OUTPUT);
    setRgbColor(0, 0, 0);

    analogReadResolution(12);
}

void UserInterface::update() {
    updateLeds();
    
    if (millis() - lastBatteryCheck > BATTERY_CHECK_MS) {
        readBattery();
        lastBatteryCheck = millis();
    }
}

void UserInterface::setLedPattern(LedPattern pattern) {
    if (currentPattern != pattern) {
        currentPattern = pattern;
        lastBlinkTime = millis();
        ledStateHigh = false;
        updateLeds(); 
        // LOG_PRINTF("UI: Pattern set to %d\n", pattern); // Opcional: Log de cambio
    }
}

int UserInterface::getBatteryPercentage() {
    return currentBatteryLevel;
}

// --- Private ---
void UserInterface::setRgbColor(bool r, bool g, bool b) {
    // Ajustar según tu hardware (Common Cathode/Anode)
    digitalWrite(PIN_LED_RED, r);
    digitalWrite(PIN_LED_GREEN, g);
    digitalWrite(PIN_LED_BLUE, b);
}

void UserInterface::updateLeds() {
    unsigned long now = millis();
    int interval = 0;
    bool blink = false;
    bool r=0, g=0, b=0;

    switch (currentPattern) {
        case LED_OFF:           setRgbColor(0,0,0); return;
        case LED_IDLE_OFFLINE:  r=0; g=1; b=0; blink=true; interval=BLINK_SLOW_MS; break;
        case LED_IDLE_ONLINE:   r=0; g=0; b=1; blink=true; interval=BLINK_SLOW_MS; break;
        case LED_SYNCING:       r=0; g=0; b=1; blink=true; interval=BLINK_FAST_MS; break;
        case LED_SUCCESS:       r=0; g=1; b=0; blink=false; break;
        case LED_ERROR_WIFI:    r=1; g=0; b=0; blink=false; break;
        case LED_ERROR_DB:      r=1; g=0; b=0; blink=true; interval=BLINK_FAST_MS; break;
        case LED_LOW_BATTERY:   r=1; g=1; b=0; blink=true; interval=BLINK_SLOW_MS; break;
    }

    if (!blink) {
        setRgbColor(r, g, b);
    } else {
        if (now - lastBlinkTime > interval) {
            ledStateHigh = !ledStateHigh;
            lastBlinkTime = now;
        }
        setRgbColor(ledStateHigh ? r : 0, ledStateHigh ? g : 0, ledStateHigh ? b : 0);
    }
}

void UserInterface::readBattery() {
    int raw = analogRead(PIN_BATTERY_ADC);
    currentBatteryLevel = map(raw, 0, 4095, 0, 100);
    if (currentBatteryLevel < 20) {
        LOG_PRINTF("UI: Low Battery (%d%%)\n", currentBatteryLevel);
    }
}

void UserInterface::disableSwitchInterrupt() {
    detachInterrupt(digitalPinToInterrupt(PIN_MODE_SWITCH));
}

void UserInterface::enableSwitchInterrupt() {
    // Vuelve a conectar la ISR que definimos antes
    attachInterrupt(digitalPinToInterrupt(PIN_MODE_SWITCH), isrModeSwitch, CHANGE);
}

void IRAM_ATTR UserInterface::isrModeSwitch() {
   static unsigned long lastInterruptTime = 0;
    unsigned long interruptTime = millis(); // O esp_timer_get_time() para microsegundos

    // Si pasaron menos de 50ms desde el último cambio, ignóralo (es ruido/rebote)
    if (interruptTime - lastInterruptTime > 50) {
        
        bool isOnline = digitalRead(PIN_MODE_SWITCH);
        Event ev;
        ev.type = isOnline ? EVENT_MODE_ONLINE_ACTIVATED : EVENT_MODE_OFFLINE_ACTIVATED;

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        if (fsmQueue != nullptr) {
            xQueueSendFromISR(fsmQueue, &ev, &xHigherPriorityTaskWoken);
        }
        if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
    }
    
    lastInterruptTime = interruptTime;
}
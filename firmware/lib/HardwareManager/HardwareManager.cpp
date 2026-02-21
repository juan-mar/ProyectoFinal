#include "HardwareManager.h"
#include "Config.h"

HardwareManager::HardwareManager() 
    : _lastSensorCheck(0) {
    // Inicializar estados de periféricos
    _peripheralState.tagEnabled = false;
    _peripheralState.tagCalibrationMode = false;
    _peripheralState.remoteEnabled = false;
    _peripheralState.bleEnabled = false;
    
    // Inicializar estados de actuadores
    _actuatorState.solenoidActive = false;
    _actuatorState.solenoidOffTime = 0;
    _actuatorState.launcherActive = false;
    _actuatorState.launcherOffTime = 0;
    _actuatorState.currentLedPattern = LED_OFF;
    _actuatorState.ledSequenceRunning = false;
    _actuatorState.ledBlinkRate = 1000;
    _actuatorState.ledLastToggle = 0;
    _actuatorState.ledCurrentState = false;
}

void HardwareManager::init(QueueHandle_t fsmQueue) {
    _fsmQueue = fsmQueue;
    _commandQueue = xQueueCreate(HW_COMMAND_QUEUE_SIZE, sizeof(HwMessage));

    LOG_PRINTLN("HardwareManager: Initializing pins...");

    // Init Pines de Actuadores
    #if ENABLE_SOLENOID
    pinMode(PIN_SOLENOID, OUTPUT);
    digitalWrite(PIN_SOLENOID, LOW);
    LOG_PRINTLN("  - Solenoid pin initialized");
    #endif

    #if ENABLE_LAUNCHER
    pinMode(PIN_LAUNCHER, OUTPUT);
    digitalWrite(PIN_LAUNCHER, LOW);
    LOG_PRINTLN("  - Launcher pin initialized");
    #endif

    #if ENABLE_LED_CONTROL
    pinMode(PIN_LED_CONTROL, OUTPUT);
    digitalWrite(PIN_LED_CONTROL, LOW);
    LOG_PRINTLN("  - LED control pin initialized");
    #endif

    #if ENABLE_TAG_READER
    pinMode(PIN_TAG_POWER, OUTPUT);
    pinMode(PIN_TAG_MODE, OUTPUT);
    digitalWrite(PIN_TAG_POWER, LOW);
    digitalWrite(PIN_TAG_MODE, LOW);
    LOG_PRINTLN("  - TAG/RFID pins initialized");
    #endif

    #if ENABLE_REMOTE_CONTROL
    pinMode(PIN_REMOTE_POWER, OUTPUT);
    digitalWrite(PIN_REMOTE_POWER, LOW);
    LOG_PRINTLN("  - Remote control pin initialized");
    #endif

    // TODO: Init Drivers (Sin prenderlos aún)
    //_remoteControl.init(); 
    //_bleScanner.init(_fsmQueue);

    LOG_PRINTLN("HardwareManager: Initialization complete");
}

bool HardwareManager::sendCommand(HwCmdType cmd, int param) {
    HwMessage msg = {cmd, param, millis()};
    // Enviamos con tiempo de espera 0 (si la cola está llena, descartamos para no bloquear FSM)
    bool result = xQueueSend(_commandQueue, &msg, 0) == pdTRUE;
    
    if (!result) {
        LOG_PRINTF("[HW] WARNING: Command queue full! Dropped command: %d\n", cmd);
    }
    
    return result;
}

void HardwareManager::update() {
    HwMessage msg;

    while (xQueueReceive(_commandQueue, &msg, 0) == pdTRUE) {
        processCommand(msg);
    }

    updateActuators();

    checkDrivers();

    if (millis() - _lastSensorCheck > 1000) {
        readSensors();
        _lastSensorCheck = millis();
    }
}

Event HardwareManager::enterLightSleep() {
    // 1. CONFIGURACIÓN (Hardware Specific)
    // Aseguramos que el pin esté listo para leer
    pinMode(PIN_MODE_SWITCH, INPUT_PULLUP); // Ajustar PCB
    
    // Lógica para despertar con el flanco contrario
    int currentState = digitalRead(PIN_MODE_SWITCH);
    gpio_int_type_t wakeupLevel = (currentState == HIGH) ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL;

    // API específica de ESP32
    gpio_wakeup_enable((gpio_num_t)PIN_MODE_SWITCH, wakeupLevel);
    esp_sleep_enable_gpio_wakeup();

    LOG_PRINTLN("HW: Entering Light Sleep...");
    LOG_FLUSH(); // Vaciar buffer serial antes de cortar reloj

    // 2. DORMIR (El procesador se detiene aquí)
    esp_light_sleep_start();

    // --------------------------------------------------
    // EL TIEMPO SE DETIENE AQUÍ HASTA EL DESPERTAR
    // --------------------------------------------------

    LOG_PRINTLN("HW: Woke up!");

    // 3. INTERPRETACIÓN (Traducción HW -> Lógica)
    // Ya no nos importa "cómo" despertó (si fue timer, gpio o uart),
    // lo que importa es el estado actual del Switch para la FSM.
    
    bool isNowOnline = digitalRead(PIN_MODE_SWITCH) == LOW; // Asumiendo LOW = ON (Pullup)
    
    // Construimos el evento agnóstico
    Event ev;
    if (isNowOnline) {
        ev.type = EVENT_MODE_ONLINE_ACTIVATED;
    } else {
        ev.type = EVENT_MODE_OFFLINE_ACTIVATED;
    }
    
    // Retornamos el evento directamente
    return ev;
}

void HardwareManager::prepareForWakeUp() {
    // 1. Limpieza de API de ESP32 (Lo que antes tenías en el exit)
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    
    // 2. Reactivar interrupciones o lógica de entrada
    // O si cambiaste a polling en el update(), quizás solo necesites resetear variables
    LOG_PRINTLN("HW: Wakeup sources disabled. Ready for activity.");
}

void HardwareManager::processCommand(HwMessage msg) {
    switch(msg.command) {
        // --- TAG / RFID CONTROL ---
        case CMD_TAG_POWER_ON:
            #if ENABLE_TAG_READER
            enableTag(msg.parameter == 1);  // parameter: 0=detección, 1=calibración
            #endif
            break;

        case CMD_TAG_POWER_OFF:
            #if ENABLE_TAG_READER
            disableTag();
            #endif
            break;

        case CMD_TAG_CALIBRATION_MODE:
            #if ENABLE_TAG_READER
            if (_peripheralState.tagEnabled) {
                _peripheralState.tagCalibrationMode = true;
                digitalWrite(PIN_TAG_MODE, HIGH);
                LOG_PRINTLN("[HW] TAG: Calibration mode enabled");
            }
            #endif
            break;

        case CMD_TAG_DETECTION_MODE:
            #if ENABLE_TAG_READER
            if (_peripheralState.tagEnabled) {
                _peripheralState.tagCalibrationMode = false;
                digitalWrite(PIN_TAG_MODE, LOW);
                LOG_PRINTLN("[HW] TAG: Detection mode enabled");
            }
            #endif
            break;

        // --- REMOTE CONTROL ---
        case CMD_REMOTE_POWER_ON:
            #if ENABLE_REMOTE_CONTROL
            if (!_peripheralState.remoteEnabled) {
                digitalWrite(PIN_REMOTE_POWER, HIGH);
                _peripheralState.remoteEnabled = true;
                LOG_PRINTLN("[HW] Remote control powered ON");
            }
            #endif
            break;

        case CMD_REMOTE_POWER_OFF:
            #if ENABLE_REMOTE_CONTROL
            if (_peripheralState.remoteEnabled) {
                digitalWrite(PIN_REMOTE_POWER, LOW);
                _peripheralState.remoteEnabled = false;
                LOG_PRINTLN("[HW] Remote control powered OFF");
            }
            #endif
            break;

        // --- LED CONTROL ---
        case CMD_LED_SEQUENCE_START:
            #if ENABLE_LED_CONTROL
            startLedSequence((LedPattern)msg.parameter, LED_CALIBRATION_BLINK_RATE_MS);
            #endif
            break;

        case CMD_LED_SEQUENCE_STOP:
            #if ENABLE_LED_CONTROL
            stopLedSequence();
            #endif
            break;

        case CMD_LED_SET_PATTERN:
            #if ENABLE_LED_CONTROL
            setLedPattern((LedPattern)msg.parameter);
            #endif
            break;

        case CMD_LED_OFF:
            #if ENABLE_LED_CONTROL
            setLedPattern(LED_OFF);
            #endif
            break;

        // --- SOLENOID / REWARD DISPENSER ---
        case CMD_SOLENOID_FIRE:
            #if ENABLE_SOLENOID
            fireSolenoid(SOLENOID_PULSE_DURATION_MS);
            #endif
            break;

        case CMD_SOLENOID_SINGLE_PULSE:
            #if ENABLE_SOLENOID
            fireSolenoid(msg.parameter > 0 ? msg.parameter : SOLENOID_PULSE_DURATION_MS);
            #endif
            break;

        // --- LAUNCHER CONTROL ---
        case CMD_LAUNCHER_ON:
            #if ENABLE_LAUNCHER
            digitalWrite(PIN_LAUNCHER, HIGH);
            _actuatorState.launcherActive = true;
            LOG_PRINTLN("[HW] Launcher powered ON");
            #endif
            break;

        case CMD_LAUNCHER_OFF:
            #if ENABLE_LAUNCHER
            digitalWrite(PIN_LAUNCHER, LOW);
            _actuatorState.launcherActive = false;
            LOG_PRINTLN("[HW] Launcher powered OFF");
            #endif
            break;

        // --- BLE ---
        case CMD_ENABLE_BLE:
            #if ENABLE_BLE_SCANNER
            if (!_peripheralState.bleEnabled) {
                _peripheralState.bleEnabled = true;
                LOG_PRINTLN("[HW] BLE Scanner enabled");
            }
            #endif
            break;

        case CMD_DISABLE_BLE:
            #if ENABLE_BLE_SCANNER
            if (_peripheralState.bleEnabled) {
                _peripheralState.bleEnabled = false;
                LOG_PRINTLN("[HW] BLE Scanner disabled");
            }
            #endif
            break;

        default:
            break;
    }
}

void HardwareManager::updateActuators() {
    updateSolenoid();
    updateLauncher();
    updateLeds();
}

void HardwareManager::updateSolenoid() {
    #if ENABLE_SOLENOID
    if (_actuatorState.solenoidActive && millis() >= _actuatorState.solenoidOffTime) {
        digitalWrite(PIN_SOLENOID, LOW);
        _actuatorState.solenoidActive = false;
        LOG_PRINTLN("[HW] Solenoid pulse completed");
    }
    #endif
}

void HardwareManager::updateLauncher() {
    #if ENABLE_LAUNCHER
    if (_actuatorState.launcherActive && millis() >= _actuatorState.launcherOffTime) {
        digitalWrite(PIN_LAUNCHER, LOW);
        _actuatorState.launcherActive = false;
        LOG_PRINTLN("[HW] Launcher fire pulse completed");
    }
    #endif
}

void HardwareManager::updateLeds() {
    #if ENABLE_LED_CONTROL
    if (_actuatorState.ledSequenceRunning) {
        unsigned long now = millis();
        if (now - _actuatorState.ledLastToggle >= _actuatorState.ledBlinkRate) {
            _actuatorState.ledCurrentState = !_actuatorState.ledCurrentState;
            digitalWrite(PIN_LED_CONTROL, _actuatorState.ledCurrentState ? HIGH : LOW);
            _actuatorState.ledLastToggle = now;
        }
    }
    #endif
}

// --- MÉTODOS AUXILIARES ESPECÍFICOS ---

void HardwareManager::enableTag(bool calibrationMode) {
    #if ENABLE_TAG_READER
    if (!_peripheralState.tagEnabled) {
        digitalWrite(PIN_TAG_POWER, HIGH);
        _peripheralState.tagEnabled = true;
        
        // Configurar modo
        _peripheralState.tagCalibrationMode = calibrationMode;
        digitalWrite(PIN_TAG_MODE, calibrationMode ? HIGH : LOW);
        
        LOG_PRINTF("[HW] TAG powered ON (mode: %s)\n", 
                   calibrationMode ? "CALIBRATION" : "DETECTION");
    }
    #endif
}

void HardwareManager::disableTag() {
    #if ENABLE_TAG_READER
    if (_peripheralState.tagEnabled) {
        digitalWrite(PIN_TAG_POWER, LOW);
        _peripheralState.tagEnabled = false;
        _peripheralState.tagCalibrationMode = false;
        LOG_PRINTLN("[HW] TAG powered OFF");
    }
    #endif
}

void HardwareManager::fireSolenoid(unsigned long durationMs) {
    #if ENABLE_SOLENOID
    if (!_actuatorState.solenoidActive) {
        digitalWrite(PIN_SOLENOID, HIGH);
        _actuatorState.solenoidActive = true;
        _actuatorState.solenoidOffTime = millis() + durationMs;
        LOG_PRINTF("[HW] Solenoid fire initiated (duration: %lu ms)\n", durationMs);
    }
    #endif
}

void HardwareManager::setLedPattern(LedPattern pattern) {
    #if ENABLE_LED_CONTROL
    stopLedSequence();  // Detener cualquier secuencia en curso
    _actuatorState.currentLedPattern = pattern;
    
    switch (pattern) {
        case LED_OFF:
            digitalWrite(PIN_LED_CONTROL, LOW);
            _actuatorState.ledCurrentState = false;
            LOG_PRINTLN("[HW] LED Pattern: OFF");
            break;
        case LED_IDLE:
            startLedSequence(LED_IDLE, LED_IDLE_BLINK_RATE_MS);
            LOG_PRINTLN("[HW] LED Pattern: IDLE");
            break;
        case LED_ACTIVE:
            digitalWrite(PIN_LED_CONTROL, HIGH);
            _actuatorState.ledCurrentState = true;
            LOG_PRINTLN("[HW] LED Pattern: ACTIVE");
            break;
        case LED_ERROR:
            startLedSequence(LED_ERROR, LED_ERROR_BLINK_RATE_MS);
            LOG_PRINTLN("[HW] LED Pattern: ERROR");
            break;
        case LED_SUCCESS:
            startLedSequence(LED_SUCCESS, LED_SUCCESS_BLINK_RATE_MS);
            LOG_PRINTLN("[HW] LED Pattern: SUCCESS");
            break;
        default:
            LOG_PRINTLN("[HW] LED Pattern: UNKNOWN");
            break;
    }
    #endif
}

void HardwareManager::startLedSequence(LedPattern pattern, unsigned long blinkRate) {
    #if ENABLE_LED_CONTROL
    _actuatorState.currentLedPattern = pattern;
    _actuatorState.ledSequenceRunning = true;
    _actuatorState.ledBlinkRate = blinkRate;
    _actuatorState.ledLastToggle = millis();
    _actuatorState.ledCurrentState = true;
    digitalWrite(PIN_LED_CONTROL, HIGH);
    LOG_PRINTF("[HW] LED Sequence started (blink rate: %lu ms)\n", blinkRate);
    #endif
}

void HardwareManager::stopLedSequence() {
    #if ENABLE_LED_CONTROL
    _actuatorState.ledSequenceRunning = false;
    digitalWrite(PIN_LED_CONTROL, LOW);
    _actuatorState.ledCurrentState = false;
    LOG_PRINTLN("[HW] LED Sequence stopped");
    #endif
}

void HardwareManager::checkDrivers() {
    // Solo chequeamos NRF24 si está habilitado
    if (_peripheralState.remoteEnabled) {
       /*
       if (_remoteControl.checkInput()) { // checkInput devuelve true si hay datos válidos
            // ¡EVENTO DETECTADO! Avisar a la FSM
            Event ev; 
          //  ev.type = EVENT_TRIGGER_DETECTED;
          //  ev.data = SOURCE_REMOTE; 
            xQueueSend(_fsmQueue, &ev, 0);
        }

       
       */     
    }
    // Nota: El BLE no se chequea aquí porque corre en su propia Task y manda eventos directo
    if(_peripheralState.bleEnabled){

    }
}

void HardwareManager::readSensors() {
    // Leer Batería
    // Leer BME280
    // Si la batería es crítica -> xQueueSend(_fsmQueue, EVENT_BATTERY_CRITICAL...)
}
#include "HardwareManager.h"
#include "Config.h"
#include <driver/uart.h>
#include <hal/uart_types.h> 

HardwareManager::HardwareManager() 
    : _lastSensorCheck(0), _bleScanner(MAC_ADDR) {
    // Inicializar estados de periféricos
    _peripheralState.tagEnabled = false;
    _peripheralState.remoteEnabled = false;
    
    // Inicializar estados de mode switches
    _modeSwitchState.prevStateA = false;
    _modeSwitchState.prevStateM = false;
    
    // Inicializar estados de actuadores
    _actuatorState.solenoidActive = false;
    _actuatorState.solenoidOffTime = 0;
    _actuatorState.launcherActive = false;
    _actuatorState.launcherEN1OnTime = 0;
    _actuatorState.launcherEN2Pending = false;
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
    pinMode(PIN_LAUNCHER_1, OUTPUT);
    pinMode(PIN_LAUNCHER_2, OUTPUT);
    digitalWrite(PIN_LAUNCHER_1, LOW);
    digitalWrite(PIN_LAUNCHER_2, LOW);
    LOG_PRINTLN("  - Launcher pins initialized");
    #endif

    #if ENABLE_LED_CONTROL
    pinMode(PIN_LED_CONTROL, OUTPUT);
    digitalWrite(PIN_LED_CONTROL, LOW);
    LOG_PRINTLN("  - LED control pin initialized");
    #endif

    #if ENABLE_TAG_READER
    LOG_PRINTLN("  - TAG/RFID pins initialized");
    //TAG interno - BLE
    #endif

    #if ENABLE_REMOTE_CONTROL
    if (_remoteControl.init()) {
        LOG_PRINTLN("[Hardware] NRF24 Control Remoto inicializado OK.");
    } else {
        LOG_PRINTLN("[Hardware] ERROR: Falló el NRF24.");
    }
    _peripheralState.remoteEnabled = false;
    #endif

    #if ENABLE_BATTERY_MONITOR
    if (_batteryMonitor.begin()) {
        LOG_PRINTLN("  - Battery Monitor initialized");
    } else {
        LOG_PRINTLN("  - WARNING: Battery Monitor init failed");
    }
    #endif
    
    // Init Mode Switch pins
    pinMode(PIN_MODE_SWITCH_A, INPUT_PULLUP);
    pinMode(PIN_MODE_SWITCH_M, INPUT_PULLUP);
    _modeSwitchState.prevStateA = digitalRead(PIN_MODE_SWITCH_A);
    _modeSwitchState.prevStateM = digitalRead(PIN_MODE_SWITCH_M);
    LOG_PRINTLN("  - Mode Switch pins initialized");
    
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
    
    checkModeSwitches();

    if (millis() - _lastSensorCheck > 1000) {
        readSensors();
        _lastSensorCheck = millis();
    }
}

void HardwareManager::updateActuators() {
    updateSolenoid();
    updateLauncher();
    updateLeds();
}

void HardwareManager::updateSolenoid() {
    #if ENABLE_SOLENOID
    if (_actuatorState.solenoidActive &&
        (long)(millis() - _actuatorState.solenoidOffTime) >= 0) {
        digitalWrite(PIN_SOLENOID, LOW);
        _actuatorState.solenoidActive = false;
        LOG_PRINTLN("[HW] Solenoid pulse completed");
    }
    #endif
}

void HardwareManager::updateLauncher() {
    #if ENABLE_LAUNCHER
    if (_actuatorState.launcherActive && _actuatorState.launcherEN2Pending &&
        millis() - _actuatorState.launcherEN1OnTime >= LAUNCHER_EN2_DELAY_MS) {
        digitalWrite(PIN_LAUNCHER_2, HIGH);
        _actuatorState.launcherEN2Pending = false;
        LOG_PRINTLN("[HW] Launcher EN_2 activated");
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

void HardwareManager::checkDrivers() {
    // Solo chequeamos NRF24 si está habilitado
    if (_peripheralState.remoteEnabled) {
      update_remote();
    }
    if(_peripheralState.tagEnabled){
        update_tag();
    }
}

void HardwareManager::update_remote() {
    int remoteCmd = _remoteControl.checkForCommand();
    
    switch (remoteCmd) {
        case CMD_REMOTE_SUCCESS:
        {
            LOG_PRINTLN("[HW] Control Remoto: BIEN");
            Event evt = {EVENT_DOG_DETECTED, 0};
            xQueueSend(_fsmQueue, &evt, 0);
            break;
        }
        
        case CMD_REMOTE_FAIL:
        {
            LOG_PRINTLN("[HW] Control Remoto: MAL");
            Event evt = {EVENT_DOG_LOST, 0};
            xQueueSend(_fsmQueue, &evt, 0);
            break;
        }
        
        case CMD_REMOTE_EXIT:
        {
            LOG_PRINTLN("[HW] Control Remoto: FIN");
            Event evt = {EVENT_PLAY_FINISHED, 0};
            xQueueSend(_fsmQueue, &evt, 0);
            break;
        }

        case CMD_REMOTE_NONE:
        default:
            break;
    }
}

void HardwareManager::update_tag() {
    switch(_bleScanner.scan()){
        case CALIBRATING:
            //LOG_PRINTLN("[HW] BLE Scanner: CALIBRATING");
            break;
        case CALIB_OK:
        {
            LOG_PRINTLN("[HW] BLE Scanner: CALIBRATION OK");
            //SEND EVENT TO FSM IF NEEDED
            Event ev;
            ev.type = EVENT_CALIBRATION_COMPLETE;
            xQueueSend(_fsmQueue, &ev, 0);
            break;
        }            
        case DETECT_FAIL: //buscando can - fuera de zona
        {
            LOG_PRINTLN("[HW] BLE Scanner: LOST!");
            Event ev;
            ev.type = EVENT_DOG_LOST;
            xQueueSend(_fsmQueue, &ev, 0);
            break;
        }
        case DETECT_OK: //can dectectado - entró a zona
        {
            LOG_PRINTLN("[HW] BLE Scanner: DETECTED!");
            Event ev;
            ev.type = EVENT_DOG_DETECTED;
            xQueueSend(_fsmQueue, &ev, 0);
            break;
        }
        default:
            break;
    }
}

void HardwareManager::readSensors() {
    // Leer Batería
    // Leer BME280
    // Si la batería es crítica -> xQueueSend(_fsmQueue, EVENT_BATTERY_CRITICAL...)
}

void HardwareManager::checkModeSwitches() {
    // Leer estado actual de los pines
    bool currentStateA = digitalRead(PIN_MODE_SWITCH_A);
    bool currentStateM = digitalRead(PIN_MODE_SWITCH_M);
    
    // Detectar flanco en PIN_MODE_SWITCH_A
    if (currentStateA != _modeSwitchState.prevStateA) {
        if (currentStateA == HIGH) {
            // Transición LOW -> HIGH: Modo ONLINE
            Event evt = {EVENT_MODE_ONLINE_ACTIVATED, 0};
            xQueueSend(_fsmQueue, &evt, 0);
            LOG_PRINTLN("[HW] Mode Switch A: ONLINE activated (LOW->HIGH)");
        } else {
            // Transición HIGH -> LOW: Modo OFFLINE
            Event evt = {EVENT_MODE_OFFLINE_ACTIVATED, 0};
            xQueueSend(_fsmQueue, &evt, 0);
            LOG_PRINTLN("[HW] Mode Switch A: OFFLINE activated (HIGH->LOW)");
        }
        _modeSwitchState.prevStateA = currentStateA;
    }
    
    // Detectar flanco en PIN_MODE_SWITCH_M (si necesitas eventos para este también)
    if (currentStateM != _modeSwitchState.prevStateM) {
        // Agregar lógica aquí si PIN_MODE_SWITCH_M también debe generar eventos
        _modeSwitchState.prevStateM = currentStateM;
    }
}


/****************************************************************
 * INTERPRETACION DE COMANDOS
 ****************************************************************/
void HardwareManager::processCommand(HwMessage msg) {
    switch(msg.command) {
        // --- TAG / RFID CONTROL ---
        case CMD_TAG_POWER_ON:
            #if ENABLE_TAG_READER
            _bleScanner.init(); // Aseguramos que el driver esté inicializado
            enableTag(msg.parameter == CMD_TAG_PARAM_CALIBRATION);  // parameter: 0=detección, 1=calibración
            _peripheralState.tagEnabled = true;
            #endif
            break;

        case CMD_TAG_POWER_OFF:
            #if ENABLE_TAG_READER
            disableTag();            
            #endif
            break;

        // --- REMOTE CONTROL ---
        case CMD_REMOTE_POWER_ON:
            #if ENABLE_REMOTE_CONTROL
            if (!_peripheralState.remoteEnabled) {
                _peripheralState.remoteEnabled = true;
                LOG_PRINTLN("[HW] Remote control powered ON");
            }
            #endif
            break;

        case CMD_REMOTE_POWER_OFF:
            #if ENABLE_REMOTE_CONTROL
            if (_peripheralState.remoteEnabled) {
                _peripheralState.remoteEnabled = false;
                _remoteControl.sleep(); 
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
            LOG_PRINTLN("[HW] Solenoid fire command received");
            #endif
            break;

        // --- LAUNCHER CONTROL ---
        case CMD_LAUNCHER_ON:
            #if ENABLE_LAUNCHER
            // Solo inicia la secuencia si estaba apagado
            if (!_actuatorState.launcherActive) {
                digitalWrite(PIN_LAUNCHER_1, HIGH);
                _actuatorState.launcherActive = true;
                _actuatorState.launcherEN1OnTime = millis();
                _actuatorState.launcherEN2Pending = true;
                LOG_PRINTLN("[HW] Launcher ON: EN_1 activated");
            } else {
                LOG_PRINTLN("[HW] Launcher: Already active, ignoring CMD_LAUNCHER_ON");
            }
            #endif
            break;

        case CMD_LAUNCHER_OFF:
            #if ENABLE_LAUNCHER
            digitalWrite(PIN_LAUNCHER_1, LOW);
            digitalWrite(PIN_LAUNCHER_2, LOW);
            _actuatorState.launcherActive = false;
            _actuatorState.launcherEN2Pending = false;
            LOG_PRINTLN("[HW] Launcher powered OFF");
            #endif
            break;

        default:
            break;
    }
}


// --- MÉTODOS AUXILIARES ESPECÍFICOS ---
void HardwareManager::enableTag(bool calibrationMode) {
    #if ENABLE_TAG_READER
    if (!_peripheralState.tagEnabled) {
        _peripheralState.tagEnabled = true;
        _bleScanner.state = calibrationMode ? CALIBRATION_RX : DETECTION_RX; // Configuramos el estado del driver según el modo   
        LOG_PRINTF("[HW] TAG powered ON (mode: %s)\n", 
                   calibrationMode ? "CALIBRATION" : "DETECTION");
    }
    #endif
}

void HardwareManager::disableTag() {
    #if ENABLE_TAG_READER
    if (_peripheralState.tagEnabled) {
        _peripheralState.tagEnabled = false;
        _bleScanner.stop(); // Detenemos el escaneo BLE si estaba activo
        LOG_PRINTLN("[HW] TAG powered OFF");
    }
    #endif
}

void HardwareManager::fireSolenoid(unsigned long durationMs) {
    #if ENABLE_SOLENOID
    digitalWrite(PIN_SOLENOID, HIGH);
    _actuatorState.solenoidActive = true;
    _actuatorState.solenoidOffTime = millis() + durationMs;
    LOG_PRINTF("[HW] Solenoid fire initiated (duration: %lu ms)\n", durationMs);
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


/****************************************************************
 * HARDWARE SLEEP MANAGEMENT
 ****************************************************************/
Event HardwareManager::enterLightSleep() {
    // 1. CONFIGURACIÓN (Hardware Specific)
    // Aseguramos que el pin esté listo para leer
    pinMode(PIN_MODE_SWITCH_A, INPUT_PULLUP); // Ajustar PCB
    
    // Lógica para despertar con el flanco contrario
    int currentState = digitalRead(PIN_MODE_SWITCH_A);
    gpio_int_type_t wakeupLevel = (currentState == HIGH) ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL;

    

    // API específica de ESP32
    gpio_wakeup_enable((gpio_num_t)PIN_MODE_SWITCH_A, wakeupLevel);
    esp_sleep_enable_gpio_wakeup();

    LOG_PRINTLN("HW: Entering Light Sleep...");
    LOG_FLUSH(); // Vaciar buffer serial antes de cortar reloj

    #ifdef DEBUG_MODE  // Asumiendo que tienes un #define DEBUG_MODE en tu config.h
    LOG_PRINTLN("[HW] Configuracion WakeUp por UART (Monitor Serie) activada.");
    
    // Le decimos que despierte si recibe al menos 3 caracteres (ej: apretar 'c' + Enter)
    uart_set_wakeup_threshold(0, 3); 
    esp_sleep_enable_uart_wakeup(0);
    #endif

    // 2. DORMIR (El procesador se detiene aquí)
    esp_light_sleep_start();

    // --------------------------------------------------
    // EL TIEMPO SE DETIENE AQUÍ HASTA EL DESPERTAR
    // --------------------------------------------------
    LOG_PRINTLN("HW: Woke up!");

    // 3. Estado de arranque    
    bool isNowOnline = digitalRead(PIN_MODE_SWITCH_A) == LOW; // Asumiendo LOW = ON (Pullup)
    
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
    // 1. Limpieza de API de ESP32
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    
    // 2. Reactivar interrupciones o lógica de entrada
    LOG_PRINTLN("HW: Wakeup sources disabled. Ready for activity.");
}

int HardwareManager::getBatteryPercentage() {
    if (!_batteryMonitor.isInitialized()) {
        LOG_PRINTLN("[HW] Battery Monitor not initialized");
        return -1;
    }
    
    BatteryInfo info = _batteryMonitor.getInfo();
    return info.percentage;
}

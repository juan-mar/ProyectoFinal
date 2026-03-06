#include "HardwareManager.h"
#include "Config.h"
#include "EventLogger.h"
#include <driver/uart.h>
#include <hal/uart_types.h> 

HardwareManager::HardwareManager() 
    : _lastBatteryReading(0), _lastEnvironmentReading(0), _bleScanner(MAC_ADDR) {
    // Inicializar queue de comandos (independiente del resto del sistema)
    _commandQueue = xQueueCreate(HW_COMMAND_QUEUE_SIZE, sizeof(HwMessage));
    if (_commandQueue == NULL) {
        LOG_PRINTLN("FATAL: Could not create HardwareManager command queue!");
        while(1);  // Halt
    }
    
    // Inicializar estados de periféricos
    _peripheralState.tagEnabled = false;
    _peripheralState.remoteEnabled = false;
    
    // Inicializar estados de mode switches
    _modeSwitchState.prevStateA = false;
    _modeSwitchState.prevStateM = false;
    
    // Inicializar estados de control remoto (doble BTN1)
    _remoteButtonState.btn1Pending = false;
    _remoteButtonState.btn1FirstPressTime = 0;
    
    // Inicializar estados de actuadores
    _actuatorState.solenoidActive = false;
    _actuatorState.solenoidOffTime = 0;
    _actuatorState.solenoidCooldownActive = false;
    _actuatorState.solenoidCooldownStartMs = 0;
    
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
        EVENT_INFO("HW remote initialized");
    } else {
        LOG_PRINTLN("[Hardware] ERROR: Falló el NRF24.");
        EVENT_ERROR("HW remote init failed");
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
    
    // Init Environment Sensor (BME280)
    #if ENABLE_ENVIRONMENT_SENSOR
    if (_environmentSensor.init()) {
        LOG_PRINTLN("  - Environment Sensor (BME280) initialized");
        EVENT_INFO("HW environment sensor initialized");
    } else {
        LOG_PRINTLN("  - WARNING: Environment Sensor init failed");
        EVENT_WARN("HW environment sensor init failed");
    }
    #endif

    // Init Mode Switch pins
    #if ENABLE_MODE_SWITCH
    pinMode(PIN_MODE_SWITCH_A, INPUT_PULLUP);
    pinMode(PIN_MODE_SWITCH_M, INPUT_PULLUP);
    _modeSwitchState.prevStateA = digitalRead(PIN_MODE_SWITCH_A);
    _modeSwitchState.prevStateM = digitalRead(PIN_MODE_SWITCH_M);
    LOG_PRINTLN("  - Mode Switch pins initialized");
    #endif

    LOG_PRINTLN("HardwareManager: Initialization complete");
}

bool HardwareManager::sendCommand(HwCmdType cmd, int param) {
    HwMessage msg = {cmd, param, millis()};
    // Enviamos con tiempo de espera 0 (si la cola está llena, descartamos para no bloquear FSM)
    bool result = xQueueSend(_commandQueue, &msg, 0) == pdTRUE;
    
    if (!result) {
        LOG_PRINTF("[HW] WARNING: Command queue full! Dropped command: %d\n", cmd);
        EVENT_WARN("HW command queue full");
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

    readSensors();
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
    unsigned long now = millis();
    
    // Verificar si expiró el tiempo de espera del BTN1
    if (_remoteButtonState.btn1Pending) {
        if (now - _remoteButtonState.btn1FirstPressTime >= _remoteButtonState.BTN1_DOUBLE_PRESS_WINDOW_MS) {
            // Pasó 1 segundo, fue un BTN1 simple (ya procesado)
            LOG_PRINTF("[HW][t=%lu] BTN1: Ventana de doble pulsación expiró (fue simple)\n", now);
            _remoteButtonState.btn1Pending = false;
        }
    }
    
    int remoteCmd = _remoteControl.checkForCommand();
    
    switch (remoteCmd) {
        case CMD_REMOTE_SUCCESS: // BTN1
        {
            if (_remoteButtonState.btn1Pending) {
                // ¡DOBLE PULSACIÓN DETECTADA!
                unsigned long deltaMs = now - _remoteButtonState.btn1FirstPressTime;
                LOG_PRINTF("[HW][t=%lu] *** DOBLE BTN1 DETECTADO (delta=%lums) -> FIN ENTRENAMIENTO ***\n", 
                           now, deltaMs);
                EVENT_INFO("Remote double BTN1 -> PLAY_FINISHED");
                
                // Resetear estado
                _remoteButtonState.btn1Pending = false;
                
                // Enviar evento de FIN del entrenamiento
                Event evt = {EVENT_PLAY_FINISHED, 0};
                xQueueSend(_fsmQueue, &evt, 0);
                
            } else {
                // Primera pulsación de BTN1
                LOG_PRINTF("[HW][t=%lu] BTN1: Primera pulsación -> DETECTADO (esperando 1s para confirmar simple/doble)\n", now);
                EVENT_INFO("Remote BTN1 -> DOG_DETECTED");
                
                _remoteButtonState.btn1Pending = true;
                _remoteButtonState.btn1FirstPressTime = now;
                
                // Enviar evento inmediato a la FSM (el perro fue detectado)
                Event evt = {EVENT_DOG_DETECTED, 0};
                xQueueSend(_fsmQueue, &evt, 0);
            }
            break;
        }
        
        case CMD_REMOTE_FAIL: // BTN2
        {
            LOG_PRINTF("[HW][t=%lu] BTN2 (MAL) -> DOG_LOST\n", now);
            EVENT_WARN("Remote BTN2 -> DOG_LOST");
            
            // Cancelar cualquier BTN1 pendiente
            _remoteButtonState.btn1Pending = false;
            
            Event evt = {EVENT_DOG_LOST, 0};
            xQueueSend(_fsmQueue, &evt, 0);
            break;
        }
        
        case CMD_REMOTE_EXIT: // FIN
        {
            LOG_PRINTF("[HW][t=%lu] FIN (Control Remoto) -> PLAY_FINISHED\n", now);
            EVENT_INFO("Remote FIN -> PLAY_FINISHED");
            
            // Cancelar cualquier BTN1 pendiente
            _remoteButtonState.btn1Pending = false;
            
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
            EVENT_INFO("BLE calibration OK");
            //SEND EVENT TO FSM IF NEEDED
            Event ev;
            ev.type = EVENT_CALIBRATION_COMPLETE;
            xQueueSend(_fsmQueue, &ev, 0);
            break;
        }            
        case DETECT_FAIL: //buscando can - fuera de zona
        {
            LOG_PRINTLN("[HW] BLE Scanner: LOST!");
            EVENT_WARN("BLE DETECT_FAIL -> DOG_LOST");
            Event ev;
            ev.type = EVENT_DOG_LOST;
            xQueueSend(_fsmQueue, &ev, 0);
            break;
        }
        case DETECT_OK: //can dectectado - entró a zona
        {
            LOG_PRINTLN("[HW] BLE Scanner: DETECTED!");
            EVENT_INFO("BLE DETECT_OK -> DOG_DETECTED");
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
    unsigned long now = millis();
    
    // Leer Batería periódicamente (según BATTERY_READ_PERIOD_MS)
    #if ENABLE_BATTERY_MONITOR
    if (now - _lastBatteryReading >= BATTERY_READ_PERIOD_MS) {
        if (_batteryMonitor.isInitialized()) {
            BatteryInfo info = _batteryMonitor.getInfo();
            
            // Si la batería es crítica, enviar evento a la FSM
            if (info.percentage < 10 && info.percentage >= 0) {
                LOG_PRINTF("[HW] WARNING: Battery critical: %d%%\n", info.percentage);
                // Event evt = {EVENT_BATTERY_CRITICAL, info.percentage};
                // xQueueSend(_fsmQueue, &evt, 0);
            }
        }
        _lastBatteryReading = now;
    }
    #endif
    
    // Leer sensor ambiental BME280 periódicamente (según ENVIRONMENT_READ_PERIOD_MS)
    if (now - _lastEnvironmentReading >= ENVIRONMENT_READ_PERIOD_MS) {
        _environmentSensor.updateReadings();
        _lastEnvironmentReading = now;
    }
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
    unsigned long now = millis();

    if (_actuatorState.solenoidCooldownActive) {
        unsigned long elapsedCooldown = now - _actuatorState.solenoidCooldownStartMs;
        if (elapsedCooldown < SOLENOID_COOLDOWN_MS) {
            unsigned long remainingMs = SOLENOID_COOLDOWN_MS - elapsedCooldown;
            LOG_PRINTF("[HW] Solenoid fire ignored (cooldown active, %lu ms remaining)\n", remainingMs);
            return;
        }

        _actuatorState.solenoidCooldownActive = false;
    }

    digitalWrite(PIN_SOLENOID, HIGH);
    _actuatorState.solenoidActive = true;
    _actuatorState.solenoidOffTime = now + durationMs;
    _actuatorState.solenoidCooldownActive = true;
    _actuatorState.solenoidCooldownStartMs = now;
    LOG_PRINTF("[HW] Solenoid fire initiated (duration: %lu ms, cooldown: %lu ms)\n", durationMs, (unsigned long)SOLENOID_COOLDOWN_MS);
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

EnvData HardwareManager::getEnvironmentData() {
    return _environmentSensor.getLastValidReadings();
}

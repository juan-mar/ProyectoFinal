#include "HardwareManager.h"
#include "Config.h"
#include "EventLogger.h"
#include "RssiLogger.h"       // RSSI signal strength logger (third output)
#include <driver/uart.h>
#include <hal/uart_types.h> 

HardwareManager::HardwareManager() 
    : _lastBatteryReading(0), _lastEnvironmentReading(0), _bleScanner(MAC_ADDR),
      _powerStatusState({false, false, false}) {
    // Inicializar queue de comandos (independiente del resto del sistema)
    _commandQueue = xQueueCreate(HW_COMMAND_QUEUE_SIZE, sizeof(HwMessage));
    if (_commandQueue == NULL) {
        LOG_PRINTLN("FATAL: Could not create HardwareManager command queue!");
        while(1);  // Halt
    }
    
    // Inicializar estados de periféricos
    _peripheralState.tagEnabled = false;
    _peripheralState.remoteEnabled = false;
    
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
}

void HardwareManager::init(QueueHandle_t fsmQueue) {
    _fsmQueue = fsmQueue;
    _messageInterface.begin();

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

    LOG_PRINTLN("  - MessageInterface initialized");

    #if ENABLE_TAG_READER
    LOG_PRINTLN("  - TAG/RFID pins initialized");
    //TAG interno - BLE
    #endif

    #if ENABLE_REMOTE_CONTROL
    if (_remoteControl.init()) {
        LOG_PRINTLN("[Hardware] NRF24 Control Remoto inicializado OK.");
        EVENT_INFO("HW:Remote OK");
    } else {
        LOG_PRINTLN("[Hardware] ERROR: Falló el NRF24.");
        EVENT_ERROR("HW:Remote FAIL");
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
        EVENT_INFO("HW:EnvSensor OK");
    } else {
        LOG_PRINTLN("  - WARNING: Environment Sensor init failed");
        EVENT_WARN("HW:EnvSensor FAIL");
    }
    #endif

    // Init Mode Switch, Power Switch, and USB detection pins
    #if ENABLE_MODE_SWITCH
    pinMode(PIN_MODE_SWITCH_ONLINE_OFFLINE, INPUT);
    pinMode(PIN_POWER_SWITCH, INPUT);
    pinMode(PIN_USB_DETECT, INPUT);
    _powerStatusState.prevModeSwitch = digitalRead(PIN_MODE_SWITCH_ONLINE_OFFLINE);
    _powerStatusState.prevPowerSwitch = digitalRead(PIN_POWER_SWITCH);
    _powerStatusState.prevUsbConnected = digitalRead(PIN_USB_DETECT);
    LOG_PRINTLN("  - Mode Switch, Power Switch, and USB detection pins initialized");
    #endif

    LOG_PRINTLN("HardwareManager: Initialization complete");
}

bool HardwareManager::sendCommand(HwCmdType cmd, int param) {
    HwMessage msg = {cmd, param, millis()};
    // Enviamos con tiempo de espera 0 (si la cola está llena, descartamos para no bloquear FSM)
    bool result = xQueueSend(_commandQueue, &msg, 0) == pdTRUE;
    
    if (!result) {
        LOG_PRINTF("[HW] WARNING: Command queue full! Dropped command: %d\n", cmd);
        EVENT_ERROR("HW Queue FULL");
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
    
    checkGPIOStatus();

    readSensors();
}

void HardwareManager::updateActuators() {
    updateSolenoid();
    updateLauncher();
    updateMessageInterface();
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

void HardwareManager::updateMessageInterface() {
    _messageInterface.update();
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
                EVENT_INFO("HW:Dbl BTN1 END");
                
                // Resetear estado
                _remoteButtonState.btn1Pending = false;
                
                // Enviar evento de FIN del entrenamiento
                Event evt = {EVENT_PLAY_FINISHED, 0};
                xQueueSend(_fsmQueue, &evt, 0);
                
            } else {
                // Primera pulsación de BTN1
                LOG_PRINTF("[HW][t=%lu] BTN1: Primera pulsación -> DETECTADO (esperando 1s para confirmar simple/doble)\n", now);
                EVENT_INFO("HW:BTN1 Success");
                
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
            EVENT_WARN("HW:BTN2 Fail");
            
            // Cancelar cualquier BTN1 pendiente
            _remoteButtonState.btn1Pending = false;
            
            Event evt = {EVENT_DOG_LOST, 0};
            xQueueSend(_fsmQueue, &evt, 0);
            break;
        }

        case CMD_REMOTE_NONE:
        default:
            break;
    }
}

void HardwareManager::update_tag() {
    #if RSSI_LOGGER_ENABLED
    RSSI_UART_POLL_DAT(RSSI_LOGGER_UART_DAT_BURST);
    #endif

    switch(_bleScanner.scan()){
        case CALIBRATING:
            //LOG_PRINTLN("[HW] BLE Scanner: CALIBRATING");
            break;
        case CALIB_OK:
        {
            LOG_PRINTLN("[HW] BLE Scanner: CALIBRATION OK");
            EVENT_INFO("HW:BLE Calib OK");
            #if RSSI_LOGGER_ENABLED
            RSSI_UART_CFG(
                _bleScanner.getKalmanQ(),
                _bleScanner.getKalmanR(),
                _bleScanner.getKalmanX0(),
                _bleScanner.getKalmanP0(),
                _bleScanner.getThreshold(),
                _bleScanner.getVarianza(),
                _bleScanner.getThreshold(),
                _bleScanner.getBarrier(),
                "OUT"
            );
            #endif
            //SEND EVENT TO FSM IF NEEDED
            Event ev;
            ev.type = EVENT_CALIBRATION_COMPLETE;
            xQueueSend(_fsmQueue, &ev, 0);
            break;
        }            
        case DETECT_FAIL: //buscando can - fuera de zona
        {
            LOG_PRINTLN("[HW] BLE Scanner: LOST!");
            EVENT_WARN("HW:BLE Dog Lost");
            #if RSSI_LOGGER_ENABLED
            RSSI_UART_EVT("OUT");
            #endif
            Event ev;
            ev.type = EVENT_DOG_LOST;
            xQueueSend(_fsmQueue, &ev, 0);
            break;
        }
        case DETECT_OK: //can dectectado - entró a zona
        {
            LOG_PRINTLN("[HW] BLE Scanner: DETECTED!");
            EVENT_INFO("HW:BLE Dog Detect");
            #if RSSI_LOGGER_ENABLED
            RSSI_UART_EVT("IN");
            #endif
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

            const char* levelText = "UNKNOWN";
            switch ((BatteryLevel)info.level) {
                case BATTERY_LEVEL_HIGH:
                    levelText = "HIGH";
                    break;
                case BATTERY_LEVEL_MEDIUM:
                    levelText = "MEDIUM";
                    break;
                case BATTERY_LEVEL_LOW:
                    levelText = "LOW";
                    break;
                case BATTERY_LEVEL_CRITICAL:
                    levelText = "CRITICAL";
                    break;
                default:
                    break;
            }

            LOG_PRINTF("[HW] Battery level=%s (%d%%, %.2fV)\n", levelText, info.percentage, info.voltage);

            if (info.isCritical && !_batteryCriticalShutdownTriggered) {
                _batteryCriticalShutdownTriggered = true;
                LOG_PRINTF("[HW] WARNING: Battery CRITICAL (%d%%). Starting shutdown sequence...\n", info.percentage);
                EVENT_ERROR("HW:Batt Critical");
                blinkRedAndShutdown();
            } else if (!info.isCritical) {
                _batteryCriticalShutdownTriggered = false;
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

void HardwareManager::checkGPIOStatus() {
    // Leer estados actuales de GPIOs críticos
    bool currentModeSwitch = digitalRead(PIN_MODE_SWITCH_ONLINE_OFFLINE);
    bool currentPowerSwitch = digitalRead(PIN_POWER_SWITCH);
    bool currentUsbConnected = digitalRead(PIN_USB_DETECT);

    // ========== SPECIAL MONITORING DURING POWEROFFSTATE ==========
    // Si estamos en PowerOffState, monitorear para eventos específicos
    if (_isInPowerOffState) {
        // ¿Se desconectó USB?
        if (currentUsbConnected == LOW && _powerStatusState.prevUsbConnected == HIGH) {
            LOG_PRINTLN("[HW] USB disconnected during PowerOff! Notifying FSM...");
            Event evt = {EVENT_POWEROFF_USB_DISCONNECTED, 0};
            xQueueSend(_fsmQueue, &evt, 0);
            EVENT_INFO("HW:USB Disc PwOff");
        }
        
        // ¿Power Switch está en LOW? (listo para dormir)
        // One-shot latch para evitar spam de eventos.
        if (currentPowerSwitch == LOW && !_powerOffReadyEventLatched) {
            LOG_PRINTLN("[HW] Power Switch released during PowerOff! Ready to sleep...");
            Event evt = {EVENT_POWEROFF_READY_TO_SLEEP, 0};
            xQueueSend(_fsmQueue, &evt, 0);
            EVENT_INFO("HW:Ready Sleep");
            _powerOffReadyEventLatched = true;
        } else if (currentPowerSwitch == HIGH) {
            _powerOffReadyEventLatched = false;
        }
    }

    // Detectar cambio en Mode Switch (Online/Offline)
    if (currentModeSwitch != _powerStatusState.prevModeSwitch) {
        if (currentModeSwitch == HIGH) {
            Event evt = {EVENT_MODE_ONLINE_ACTIVATED, 0};
            xQueueSend(_fsmQueue, &evt, 0);
            LOG_PRINTLN("[HW] Mode Switch: ONLINE activated (LOW->HIGH)");
            EVENT_INFO("HW:Mode ONLINE");
        } else {
            Event evt = {EVENT_MODE_OFFLINE_ACTIVATED, 0};
            xQueueSend(_fsmQueue, &evt, 0);
            LOG_PRINTLN("[HW] Mode Switch: OFFLINE activated (HIGH->LOW)");
            EVENT_INFO("HW:Mode OFFLINE");
        }
        _powerStatusState.prevModeSwitch = currentModeSwitch;
    }
    
    // Detectar cambio en USB (conectado/desconectado)
    if (currentUsbConnected != _powerStatusState.prevUsbConnected) {
        if (currentUsbConnected == HIGH && !_isInPowerOffState) {
            // Transición LOW -> HIGH: USB conectado (cargando)
            // (Only send event if NOT in PowerOffState - PowerOff has its own handling)
            Event evt = {EVENT_USB_CONNECTED, 0};
            xQueueSend(_fsmQueue, &evt, 0);
            LOG_PRINTLN("[HW] USB: Connected (charging started)");
            EVENT_INFO("HW:USB Charging");
        } else if (currentUsbConnected == LOW && !_isInPowerOffState) {
            // Transición HIGH -> LOW: USB desconectado
            LOG_PRINTLN("[HW] USB: Disconnected (charging stopped)");
            EVENT_INFO("HW:USB Disc");
        }
        _powerStatusState.prevUsbConnected = currentUsbConnected;
    }
    
    // Detectar Power Switch OFF por NIVEL + latch (no spam de cola).
    // Esto cubre también el caso de boot con switch ya en LOW.
    if (!_isInPowerOffState) {
        if (currentPowerSwitch == LOW && !_powerSwitchOffEventLatched) {
            Event evt = {EVENT_POWER_SWITCH_OFF, 0};
            xQueueSend(_fsmQueue, &evt, 0);
            LOG_PRINTLN("[HW] Power Switch: OFF level detected");
            EVENT_WARN("HW:Power OFF");
            _powerSwitchOffEventLatched = true;
        } else if (currentPowerSwitch == HIGH) {
            _powerSwitchOffEventLatched = false;
        }
    }

    // Guardar estado actual para detección de transiciones en próximo ciclo
    _powerStatusState.prevPowerSwitch = currentPowerSwitch;
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

        // --- USER MESSAGE INTERFACE ---
        case CMD_MSG_SET:
            _messageInterface.setMessage(static_cast<UserMessage>(msg.parameter));
            break;

        case CMD_MSG_OFF:
            _messageInterface.setMessage(USER_MSG_OFF);
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
    if (!calibrationMode) {
        // Proteccion extra: al entrar a DETECTION arrancamos "lejos" aunque venga de calibracion.
        _bleScanner.prepareDetectionStart();
    }

    if (!_peripheralState.tagEnabled) {
        _peripheralState.tagEnabled = true;
        _bleScanner.state = calibrationMode ? CALIBRATION_RX : DETECTION_RX;
        LOG_PRINTF("[HW] TAG powered ON (mode: %s)\n",
                   calibrationMode ? "CALIBRATION" : "DETECTION");
    } else {
        // Si el scanner ya estaba encendido, igual permitimos actualizar el modo.
        _bleScanner.state = calibrationMode ? CALIBRATION_RX : DETECTION_RX;
        LOG_PRINTF("[HW] TAG mode switched to: %s\n",
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

void HardwareManager::blinkRedAndShutdown() {
    _messageInterface.setMessage(USER_MSG_ERROR);
    vTaskDelay(pdMS_TO_TICKS(900));

    enterDeepSleep();
}


/****************************************************************
 * HARDWARE SLEEP MANAGEMENT
 ****************************************************************/
/****************************************************************
 * HARDWARE SLEEP MANAGEMENT
 ****************************************************************/
Event HardwareManager::enterLightSleep() {
    // 1. CONFIGURACIÓN (Hardware Specific)
    // Configuramos wakeup por: Power Switch OFF, USB conectado, y Mode Switch
    pinMode(PIN_POWER_SWITCH, INPUT);
    pinMode(PIN_USB_DETECT, INPUT);
    pinMode(PIN_MODE_SWITCH_ONLINE_OFFLINE, INPUT);
    
    int currentPowerState = digitalRead(PIN_POWER_SWITCH);
    int currentUSBState = digitalRead(PIN_USB_DETECT);
    int currentModeSwitch = digitalRead(PIN_MODE_SWITCH_ONLINE_OFFLINE);
    
    // Power Switch: despertar si está HIGH y pasa a LOW (apagado)
    if (currentPowerState == HIGH) {
        gpio_wakeup_enable((gpio_num_t)PIN_POWER_SWITCH, GPIO_INTR_LOW_LEVEL);
    }
    
    // USB: despertar si está LOW y pasa a HIGH (se conecta)
    if (currentUSBState == LOW) {
        gpio_wakeup_enable((gpio_num_t)PIN_USB_DETECT, GPIO_INTR_HIGH_LEVEL);
    }
    
    // Mode Switch: despertar con cualquier cambio (flanco contrario)
    gpio_int_type_t modeSwitchWakeupLevel = (currentModeSwitch == HIGH) ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL;
    gpio_wakeup_enable((gpio_num_t)PIN_MODE_SWITCH_ONLINE_OFFLINE, modeSwitchWakeupLevel);

    esp_sleep_enable_gpio_wakeup();

    LOG_PRINTLN("[HW] Entering Light Sleep...");
    LOG_PRINTLN("[HW] Wakeup sources: Power Switch OFF, USB Connect, or Mode Switch change");
    LOG_FLUSH(); // Vaciar buffer serial antes de cortar reloj

    #if DEBUG_MODE == 1
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
    LOG_PRINTLN("[HW] Woke up from Light Sleep!");

    // 3. Verificar QUÉ lo despertó (prioridad: Power > USB > Mode)
    bool powerSwitchNowOff = digitalRead(PIN_POWER_SWITCH) == LOW;
    bool usbNowConnected = digitalRead(PIN_USB_DETECT) == HIGH;
    bool modeSwitchNowOnline = digitalRead(PIN_MODE_SWITCH_ONLINE_OFFLINE) == HIGH;
    
    // Construimos el evento
    Event ev;
    if (powerSwitchNowOff) {
        LOG_PRINTLN("[HW] Woke up: Power Switch OFF detected");
        ev.type = EVENT_POWER_SWITCH_OFF;
    } else if (usbNowConnected) {
        LOG_PRINTLN("[HW] Woke up: USB Connected detected");
        ev.type = EVENT_USB_CONNECTED;
    } else if (modeSwitchNowOnline != (currentModeSwitch == HIGH)) {
        // Mode Switch cambió de estado
        if (modeSwitchNowOnline) {
            LOG_PRINTLN("[HW] Woke up: Mode Switch changed to ONLINE");
            ev.type = EVENT_MODE_ONLINE_ACTIVATED;
        } else {
            LOG_PRINTLN("[HW] Woke up: Mode Switch changed to OFFLINE");
            ev.type = EVENT_MODE_OFFLINE_ACTIVATED;
        }
    } else {
        // Despertó por otra razón (UART en debug, timeout, etc)
        LOG_PRINTLN("[HW] Woke up: Unknown source (UART/timeout)");
        ev.type = EVENT_NULL;
    }
    
    // Retornamos el evento directamente
    return ev;
}

void HardwareManager::enterDeepSleep() {
    LOG_PRINTLN("[HW] ======================================");
    LOG_PRINTLN("[HW] Entering DEEP SLEEP mode...");
    LOG_PRINTLN("[HW] Power consumption: ~10µA");
    LOG_PRINTLN("[HW] Wakeup source: Power Switch ON (GPIO14 HIGH)");
    LOG_PRINTLN("[HW] Note: Device will RESET on wakeup");
    LOG_PRINTLN("[HW] ======================================");
    PIN_LOW(2);
    EVENT_WARN("HW:DeepSleep NOW");
    
    // Flush all pending logs to LCD before sleep
    EVENT_FLUSH();
    
    // 1. Apagar todos los periféricos para mínimo consumo
    _messageInterface.setMessage(USER_MSG_OFF);
    
    #if ENABLE_SOLENOID
    digitalWrite(PIN_SOLENOID, LOW);
    #endif
    
    #if ENABLE_LAUNCHER
    digitalWrite(PIN_LAUNCHER_1, LOW);
    digitalWrite(PIN_LAUNCHER_2, LOW);
    #endif
    
    // 2. Apagar WiFi, Remoto, BME280, Tag (si está habilitado en la FSM)
    #if ENABLE_REMOTE_CONTROL
    if (_peripheralState.remoteEnabled) {
        sendCommand(CMD_REMOTE_POWER_OFF, 0);
        _peripheralState.remoteEnabled = false;
    }
    #endif
    
    #if ENABLE_TAG_READER
    if (_peripheralState.tagEnabled) {
        disableTag();
        _peripheralState.tagEnabled = false;
    }
    #endif
    
    #if ENABLE_ENVIRONMENT_SENSOR
    // BME280 se desactiva automáticamente con I2C inactivo
    #endif
    
    // 3. Configurar wakeup SOLO por Power Switch (GPIO14 = HIGH)
    pinMode(PIN_POWER_SWITCH, INPUT);
    int currentPowerState = digitalRead(PIN_POWER_SWITCH);
    int wakeLevel = (currentPowerState == HIGH) ? 0 : 1;
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_POWER_SWITCH, wakeLevel);
    LOG_PRINTF("[HW] Deep Sleep wakeup level set to %d (current Power Switch=%d)\n", wakeLevel, currentPowerState);
    
    LOG_PRINTLN("[HW] Deep Sleep configured. Waiting for Power Switch HIGH...");
    LOG_FLUSH();
    
    // 4. ENTRAR EN DEEP SLEEP
    // Esta función NO RETORNA - el ESP32 se reinicia completamente al despertar
    esp_deep_sleep_start();
    
    // ========================================================
    // CÓDIGO DESPUÉS DE ESTA LÍNEA NUNCA SE EJECUTA
    // Al despertar, el ESP32 hace reset y ejecuta setup()
    // ========================================================
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

float HardwareManager::getBatteryVoltage() {
    if (!_batteryMonitor.isInitialized()) {
        LOG_PRINTLN("[HW] Battery Monitor not initialized");
        return -1.0f;
    }

    BatteryInfo info = _batteryMonitor.getInfo();
    return info.voltage;
}

const char* HardwareManager::getBatteryLevelText() {
    if (!_batteryMonitor.isInitialized()) {
        LOG_PRINTLN("[HW] Battery Monitor not initialized");
        return "UNKNOWN";
    }

    BatteryInfo info = _batteryMonitor.getInfo();
    switch ((BatteryLevel)info.level) {
        case BATTERY_LEVEL_HIGH:
            return "HIGH";
        case BATTERY_LEVEL_MEDIUM:
            return "MEDIUM";
        case BATTERY_LEVEL_LOW:
            return "LOW";
        case BATTERY_LEVEL_CRITICAL:
            return "CRITICAL";
        default:
            return "UNKNOWN";
    }
}

EnvData HardwareManager::getEnvironmentData() {
    return _environmentSensor.getLastValidReadings();
}

void HardwareManager::notifyPowerOffState(bool entering) {
    _isInPowerOffState = entering;
    _powerOffReadyEventLatched = false;
    
    if (entering) {
        LOG_PRINTLN("[HW] PowerOffState ENTERED - GPIO monitoring enabled");
    } else {
        LOG_PRINTLN("[HW] PowerOffState EXITED - GPIO monitoring disabled");
    }
}

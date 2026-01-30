#include "HardwareManager.h"
#include "Config.h"

// Configuración
#define HW_QUEUE_SIZE 10
#define PIN_SOLENOIDE 26 // Redefinir
#define LOOP_DELAY_MS 20 // 50Hz: Suficiente para LEDs fluidos y respuesta rápida

HardwareManager::HardwareManager() 
    : _remoteEnabled(false), _bleEnabled(false), _solenoidActive(false) {}

void HardwareManager::init(QueueHandle_t fsmQueue) {
    _fsmQueue = fsmQueue;
    _commandQueue = xQueueCreate(HW_QUEUE_SIZE, sizeof(HwMessage));

    // Init Pines
    pinMode(PIN_SOLENOIDE, OUTPUT);
    digitalWrite(PIN_SOLENOIDE, LOW);

    // Init Drivers (Sin prenderlos aún)
    //_remoteControl.init(); 
    //_bleScanner.init(_fsmQueue); // Le pasamos la cola por si el BLE reporta directo
}

bool HardwareManager::sendCommand(HwCmdType cmd, int param) {
    HwMessage msg = {cmd, param};
    // Enviamos con tiempo de espera 0 (si la cola está llena, descartamos para no bloquear FSM)
    return xQueueSend(_commandQueue, &msg, 0) == pdTRUE;
}

void HardwareManager::update() {
    HwMessage msg;

    // 1. FASE REACTIVA: Vaciar la cola de comandos pendientes
    // Usamos '0' de tiempo de espera porque no queremos bloquear el loop
    while (xQueueReceive(_commandQueue, &msg, 0) == pdTRUE) {
        processCommand(msg);
    }

    // 2. FASE LÓGICA: Actualizar temporizadores (Solenoide, LEDs)
    updateActuators();

    // 3. FASE DRIVERS: Polling al control remoto (si está activo)
    checkDrivers();

    // 4. FASE SENSORES LENTOS: Chequeo cada 1 segundo
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
    
    // Opcional: Configurar UART wakeup si lo necesitas
    // esp_sleep_enable_uart_wakeup(0);

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
        // --- ACTUADORES ---
        case CMD_FIRE_SOLENOID:
            digitalWrite(PIN_SOLENOIDE, HIGH);
            _solenoidActive = true;
            _solenoidOffTime = millis() + 200; // Duración del disparo
            break;

        case CMD_SET_LED_PATTERN:
            // Llama a tu lógica de LEDs existente
            // self->leds.setPattern((LedPattern)msg.parameter);
            break;

        // --- GESTIÓN DE DRIVERS ---
        case CMD_ENABLE_REMOTE:
            if (!_remoteEnabled) {
                //_remoteControl.powerOn(); 
                _remoteEnabled = true;
            }
            break;

        case CMD_DISABLE_REMOTE:
            if (_remoteEnabled) {
                //_remoteControl.powerOff();
                _remoteEnabled = false;
            }
            break;

        case CMD_ENABLE_BLE:
            if (!_bleEnabled) {
                //_bleScanner.startScanning(); // Resume Task interna
                _bleEnabled = true;
            }
            break;

        case CMD_DISABLE_BLE:
            if (_bleEnabled) {
                //_bleScanner.stopScanning(); // Suspend Task interna
                _bleEnabled = false;
            }
            break;
    }
}

void HardwareManager::updateActuators() {
    // Manejo no bloqueante del Solenoide
    if (_solenoidActive && millis() >= _solenoidOffTime) {
        digitalWrite(PIN_SOLENOIDE, LOW);
        _solenoidActive = false;
    }

    // Actualizar animación de LEDs
    // self->leds.update(); 
}

void HardwareManager::checkDrivers() {
    // Solo chequeamos NRF24 si está habilitado
    if (_remoteEnabled) {
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
}

void HardwareManager::readSensors() {
    // Leer Batería
    // Leer BME280
    // Si la batería es crítica -> xQueueSend(_fsmQueue, EVENT_BATTERY_CRITICAL...)
}

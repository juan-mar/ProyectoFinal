#include "rx.h"

// Puntero auxiliar para conectar los callbacks estáticos con tu objeto
Receptor* globalReceptorRef = nullptr;

// --- CALLBACK 1: Detecta el dispositivo y actualiza la variable INSTANTÁNEAMENTE ---
// Re-hacemos el callback para que tenga la MAC objetivo dentro y sea autónomo
class FiltradoRapidoCallback: public BLEAdvertisedDeviceCallbacks {
    Receptor* _ref;
    String _macBuscada;
public:
    FiltradoRapidoCallback(Receptor* ref, String mac) {
        _ref = ref;
        _macBuscada = mac;
    }
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        String mac = advertisedDevice.getAddress().toString().c_str();
        mac.toUpperCase();
        if (mac == _macBuscada) {
            // ¡BINGO! Actualización directa a la variable
            _ref->_procesarDato(advertisedDevice.getRSSI());
        }
    }
};

// --- CALLBACK 2: El Limpiador de Memoria ---
void scanCompleteCB(BLEScanResults results) {
    if (globalReceptorRef != nullptr) {
        globalReceptorRef->_reiniciarEscaneo();
    }
}

// --- IMPLEMENTACIÓN DE LA CLASE ---

Receptor::Receptor(String targetMac) {
    _targetMac = targetMac;
    _targetMac.toUpperCase();
    rssiActual = -100; // Valor inicial "lejos"
    ultimaActualizacion = 0;
    _escaneando = false;
    globalReceptorRef = this; // Enganchamos el puntero global
}

void Receptor::init() {
    BLEDevice::init("");
    _pBLEScan = BLEDevice::getScan();
    // Usamos el callback filtrado que creamos arriba
    _pBLEScan->setAdvertisedDeviceCallbacks(new FiltradoRapidoCallback(this, _targetMac));
}

void Receptor::_procesarDato(int rssi) {
    rssiActual = rssi;
    ultimaActualizacion = millis();
}

void Receptor::_reiniciarEscaneo() {
    _escaneando = false; // Bandera abajo
    // El loop() se encargará de arrancar de nuevo
}

void Receptor::loop() {
    // Si NO está escaneando, arrancamos un nuevo ciclo.
    if (!_escaneando) {
        _pBLEScan->clearResults(); // ¡AQUÍ SE LIBERA LA MEMORIA!
        
        // start(tiempo, callback, is_continue)
        // Ponemos 5 segundos. Podría ser 1, 10 o 30. 
        // Cuanto más largo, más riesgo de llenar RAM si hay muchos dispositivos cerca.
        // 3 segundos es un balance perfecto.
        _pBLEScan->start(3, scanCompleteCB, false); 
        
        _escaneando = true;
    }
}
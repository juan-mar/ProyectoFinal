#include "rx.h"

// Inicialización del puntero estático (Obligatorio en C++ fuera de la clase)
ReceptorRF* ReceptorRF::_instancia = nullptr;

// --- Constructor ---
ReceptorRF::ReceptorRF(int pin, unsigned long tiempoMetaMs) 
  : _pin(pin), _tiempoMeta(tiempoMetaMs), _timeoutSenal(50) {
  
  // Inicializamos variables seguras
  _ultimoPulsoMs = 0;
  _pulsoDetectado = false;
  _conteoActivo = false;
  _exito = false;
  _inicioConteo = 0;
}

// --- Proxy Estático ---
// Esta función existe en memoria global y redirige al objeto actual
void IRAM_ATTR ReceptorRF::isrProxy() {
  if (_instancia) {
    _instancia->handleISR();
  }
}

// --- Manejador Real de la Interrupción ---
void IRAM_ATTR ReceptorRF::handleISR() {
    unsigned long now = micros();
    _sampleTime = now - _ultimoPulsoMs;
    if (_sampleTime > 500 && _sampleTime < 1000) {
        _pulsoDetectado = true;
    }
    _ultimoPulsoMs = now;
}

/*************************** Begin *****************************/
void ReceptorRF::begin() {
  pinMode(_pin, INPUT);
  
  _instancia = this; 
  
  // Configuramos la interrupción
  // RISING para detectar el flanco de subida
  attachInterrupt(digitalPinToInterrupt(_pin), isrProxy, RISING);
}

/************************* Actualizar ******************************/
bool ReceptorRF::actualizar() {
  unsigned long ahora = micros();
  Serial.println((ahora - _ultimoPulsoMs));

  // 1. Revisar Timeout (Pérdida de señal)
  // Si pasaron más de 3 Ts sin cambios en el pin, la señal se perdió
  if ((ahora - _ultimoPulsoMs) > (3*750)) {
    _pulsoDetectado = false;
    Serial.println("Conexion perdida");
    
    // Si estábamos contando, reiniciamos todo (a menos que ya haya sido exitoso)
    //if (!_exito) { 
    //  _conteoActivo = false;
    //  _inicioConteo = 0;
    //}
    //return false; 
  }

  /*
  // 2. Lógica de Conteo (Solo si hay señal detectada recientemente)
  if (_pulsoDetectado) {
    
    // Inicio del conteo
    if (!_conteoActivo) {
      _conteoActivo = true;
      _inicioConteo = ahora;
      _exito = false; // Nos aseguramos que no esté en estado de éxito previo
    }

    // Chequeo de Meta
    if (!_exito && (ahora - _inicioConteo >= _tiempoMeta)) {
      _exito = true;
      return true; // Se cumplió el tiempo
    }
  }*/
  
  return _pulsoDetectado;
}

/********************** Reset **********************/
void ReceptorRF::reset() {
  _exito = false;
  _conteoActivo = false;
  _inicioConteo = 0;
}
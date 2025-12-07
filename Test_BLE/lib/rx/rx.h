#ifndef RX_H
#define RX_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

class Receptor {
  public:
    // --- TU VARIABLE GLOBAL ---
    // volatile: Le dice al procesador que esta variable puede cambiar en cualquier 
    // momento (por el bluetooth) y que siempre lea el valor fresco.
    volatile int rssiActual;
    
    // Para saber si está "vivo" o se perdió la señal
    volatile unsigned long ultimaActualizacion; 

    Receptor(String targetMac);
    void init();
    
    // Esta función mantiene el ciclo infinito rodando
    void loop(); 

    // Métodos internos (públicos por simplicidad técnica)
    void _procesarDato(int rssi);
    void _reiniciarEscaneo();

  private:
    String _targetMac;
    BLEScan* _pBLEScan;
    bool _escaneando;
};

#endif
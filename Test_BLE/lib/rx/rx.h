#ifndef RX_H
#define RX_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>



class Receptor {
    private:
    String _targetMac;
    BLEScan* _pBLEScan;
    bool _escaneando;

    float threshold;
    float varianza;
    int barrier;
    bool new_msg;

    // Guarda el ultimo valor de RSSI
    volatile int rssiActual;
    // Para saber si está vivo o se perdió la señal
    volatile unsigned long ultimaActualizacion; 

    unsigned long _tiempoUltimoReinicio;


    public:


    //Constructor. Recibe la MAC address del transmisor
    Receptor(String targetMac);

    //Inicialización de los callbacks
    void init();
    
    // Esta función mantiene el ciclo infinito rodando
    void loop(); 

    // Funcion de calibracion
    bool calibracion();

    // Función de detección de umbral
    bool detect_thres();

    // Métodos internos (públicos por simplicidad técnica)
    void _procesarDato(int rssi);
    void _reiniciarEscaneo();

    int getRSSI();

    long getUltimaActualizacion(); 

    void setNewMsg(bool val);
    bool isNewMsg();

};



#endif
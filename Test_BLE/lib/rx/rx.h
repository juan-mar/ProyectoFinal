#ifndef RX_H
#define RX_H

#include <Arduino.h>
//#include <BLEDevice.h>
//#include <BLEUtils.h>
//#include <BLEScan.h>
//#include <BLEAdvertisedDevice.h>

#include <NimBLEDevice.h>

#define THRES_TIME 3000
#define MAC_ADDR "9C:1D:58:95:7B:9C" 
#define CALIBRATION_RX 1
#define DETECTION_RX 2

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

    // AGREGA ESTO: Puntero para controlar el callback
    BLEAdvertisedDeviceCallbacks* _pCallbacks = nullptr; 
    
    // AGREGA ESTO: Bandera para controlar si el sistema debe seguir funcionando
    bool _sistemaActivo = false;


    public:

    int state;  

    //Constructor. Recibe la MAC address del transmisor
    Receptor(String targetMac);

    //Inicialización de los callbacks
    void init();
    
    // Esta función mantiene el ciclo infinito rodando
    void clear(); 

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

    // AGREGA ESTO: Prototipo de la función stop
    void stop();

    void scan(); // Método para iniciar el escaneo (puede ser llamado desde loop())

};



#endif
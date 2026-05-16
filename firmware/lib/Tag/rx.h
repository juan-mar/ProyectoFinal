#ifndef RX_H
#define RX_H

#include <Arduino.h>

#include <NimBLEDevice.h>

#define THRES_TIME 3000
#define MAC_ADDR "9C:1D:58:95:7B:9C" 
#define CALIBRATION_RX 1
#define DETECTION_RX 2

enum Estados {
    CALIBRATING,
    CALIB_OK,
    DETECT_OK,      //Entró a zona
    DETECT_FAIL,    //Salió de zona
    STOP,
    IDLE
};

class Receptor {
    private:
        String _targetMac;
        BLEScan* _pBLEScan;

        float threshold;
        float varianza;
        float barrier;
        bool new_msg;

        // Guarda el ultimo valor de RSSI
        volatile int rssiActual;
        volatile float rssi_filtered;

        unsigned long _tiempoUltimoReinicio;

        // AGREGA ESTO: Puntero para controlar el callback
        BLEAdvertisedDeviceCallbacks* _pCallbacks = nullptr; 
        
        // AGREGA ESTO: Bandera para controlar si el sistema debe seguir funcionando
        bool _sistemaActivo = false;
        bool _prevStateDetected;


    public:

        int state;  

        //Constructor. Recibe la MAC address del transmisor
        Receptor(String targetMac);

        //Inicialización de los callbacks
        void init(); 

        /* Funcion de calibracion
        true: calibracion en progreso
        false: calibracion finalizada */
        bool calibracion();

        // Función de detección de umbral
        bool detect_thres();

        /* Funcion de procesamiento de datos
        Registra el valor crudo de rssi en rssiActual
        Utiliza funcion de filtrado y registra el valor en rssi_filtered*/
        void _procesarDato(int rssi);

        // Reinicia solo el estado de detección/filtro sin perder threshold calibrado.
        void prepareDetectionStart();

        int getRSSI();
        float getFilteredRSSI();
        float getThreshold() const;
        float getVarianza() const;
        float getBarrier() const;
        float getKalmanQ() const;
        float getKalmanR() const;
        float getKalmanX0() const;
        float getKalmanP0() const;

        void setCalibration(float threshold,
                float variance,
                float barrier);

        void setNewMsg(bool val);
        bool isNewMsg();

        //Prototipo de la función stop
        void stop();

        int scan(); // Método para iniciar el escaneo (puede ser llamado desde loop())

};



#endif
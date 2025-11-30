#ifndef RECEPTOR_RF_H
#define RECEPTOR_RF_H

#include <Arduino.h> // Necesario para tipos como byte, unsigned long, IRAM_ATTR, etc.

class ReceptorRF {
  private:
    /*************** Constantes de configuración ******************/
    const int _pin;
    const unsigned long _tiempoMeta;
    const unsigned long _timeoutSenal;
    
    /*************** Variables Volátiles (Interrupciones) ******************/
    volatile unsigned long _ultimoPulsoMs;
    volatile unsigned long _sampleTime;
    volatile bool _pulsoDetectado;

    /*************** Variables de Lógica de Estado ******************/
    unsigned long _inicioConteo;
    bool _conteoActivo;
    bool _exito;

    /*************** Manejo de Interrupciones en Clases ******************/
    // Puntero estático para saber qué instancia maneja la interrupción
    static ReceptorRF* _instancia;

    // Función estática que recibe la interrupción del hardware
    static void IRAM_ATTR isrProxy();

    // La función real que procesa la interrupción (método de instancia)
    void IRAM_ATTR handleISR();

  public:
    // Constructor
    // tiempoMetaMs: Tiempo necesario para éxito (default 5000ms)
    ReceptorRF(int pin, unsigned long tiempoMetaMs = 5000);

    // Configuración inicial
    void begin();

    // Lógica principal. Retorna true solo cuando se cumple el tiempo.
    bool actualizar();

    // Resetea el estado para volver a empezar
    void reset();
};

#endif
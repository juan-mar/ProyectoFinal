#ifndef REMOTE_CONTROL_H
#define REMOTE_CONTROL_H

#include <RH_NRF24.h>

// Definimos los comandos que puede enviar el control
#define CMD_REMOTE_NONE    0
#define CMD_REMOTE_SUCCESS 1  // Botón BIEN
#define CMD_REMOTE_FAIL    2  // Botón MAL
#define CMD_REMOTE_EXIT    3  // Botón FIN

class RemoteControl {


public:
    // El constructor recibe los pines CE y CSN del ESP32
    RemoteControl();
    
    bool init();
    
    // Función no bloqueante para leer el control
    int checkForCommand();
    
    // Función para poner el módulo en modo de bajo consumo
    void sleep();

private:
    RH_NRF24 nrf24;
};

#endif // REMOTE_CONTROL_H
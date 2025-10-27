/****************************************************************
 * @file NombreDelArchivo.h
 * @brief Breve descripción de lo que hace este archivo.
 ****************************************************************/

/****************************************************************
 * Include Guards
 ****************************************************************/
#ifndef NOMBRE_DEL_ARCHIVO_H
#define NOMBRE_DEL_ARCHIVO_H

/****************************************************************
 * Headers
 ****************************************************************/
// Includes de librerías de Arduino/ESP32
//#include <Arduino.h>

// Includes de otras librerías externas
// ...

// Includes de otros módulos de este proyecto
// (Intentar minimizar en archivos .h)

/****************************************************************
 * Forward Declarations
 ****************************************************************/
// class OtraClase; // Ejemplo si se necesita

/****************************************************************
 * Defines
 ****************************************************************/
// #define MI_DEFINE_PUBLICO 123

/****************************************************************
 * Types of data
 ****************************************************************/
// struct MiStruct { ... };

/****************************************************************
 * Classes / Functions prototypes
 ****************************************************************/
/**
 * @brief Breve descripción de la clase.
 */
class MiClase {
public:
    MiClase(); // Constructor
    ~MiClase(); // Destructor

    void miMetodoPublico();

private:
    int miVariablePrivada;
};

#endif // NOMBRE_DEL_ARCHIVO_H
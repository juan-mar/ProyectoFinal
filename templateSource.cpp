/****************************************************************
 * @file NombreDelArchivo.cpp
 * @brief Implementación de los métodos de la clase MiClase.
 ****************************************************************/

/****************************************************************
 * Headers
 ****************************************************************/
#include "NombreDelArchivo.h" // ¡El .h propio siempre primero!

// Includes de otros módulos necesarios para la implementación
// #include "OtroModulo.h"

/****************************************************************
 * Defines
 ****************************************************************/
// #define MI_DEFINE 456

/****************************************************************
 * Global Variables (file scope)
 ****************************************************************/
// static bool miVariableSoloParaEsteArchivo = false;

/****************************************************************
 * Methods Implementation / Function Definitions
 ****************************************************************/

/**
 * @brief Constructor de MiClase.
 */
MiClase::MiClase() {
    // Inicializar variables
    miVariablePrivada = 0;
}

/**
 * @brief Destructor de MiClase.
 */
MiClase::~MiClase() {
    // Liberar memoria si es necesario
}

/**
 * @brief Implementación de miMetodoPublico.
 */
void MiClase::miMetodoPublico() {
    // Código del método
    miVariablePrivada++;
}

// ... more functions ...
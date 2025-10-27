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

/****************************************************************
 * Forward Declarations
 ****************************************************************/

/****************************************************************
 * Defines
 ****************************************************************/

/****************************************************************
 * Types of data
 ****************************************************************/
/**
 * @brief Enumera todos los posibles tipos de eventos que pueden
 * ocurrir en el sistema y ser procesados por el GestorDeEstados.
 */
enum typeEvent {
    // --- Eventos de Transición de Estado ---
    EVENTO_NULO = 0,                // Evento inválido o por defecto
    
    EVENTO_EMPEZAR_JUEGO_MANUAL,    // Gatillado por el WebServer
    EVENTO_EMPEZAR_JUEGO_AUTOMATICO,// Gatillado por el WebServer
    EVENTO_FIN_JUEGO,               // Gatillado por ManualController o AutoController
    
    // --- Eventos de Hardware ---
    EVENTO_MODO_ONLINE_ACTIVADO,    // Gatillado por ISR del interruptor
    EVENTO_MODO_OFFLINE_ACTIVADO,   // Gatillado por ISR del interruptor
    
    // --- Eventos de Módulos Internos ---
    EVENTO_SINCRO_DATOS_TERMINADA,  // Enviado por SupabaseManager
    EVENTO_SINCRO_DATOS_FALLIDA     // Enviado por SupabaseManager
};

/**
 * @brief Estructura del objeto que se envía a través de la cola de eventos.
 */
struct event{
    typeEvent tipo;
    union {
        int     valorInt;
        float   valorFloat;
        bool    valorBool;
    } payload;
};

/****************************************************************
 * Classes / Functions prototypes
 ****************************************************************/

#endif // EVENTS_H
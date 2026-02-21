/****************************************************************
 * @file Events.h
 * @brief Defines the event types and data structure
 * used in the FSM event queue.
 ****************************************************************/

/****************************************************************
 * Include Guards
 ****************************************************************/
#ifndef EVENTS_H
#define EVENTS_H

/****************************************************************
 * Data Types (structs, enums)
 ****************************************************************/

/**
 * @brief Enumerates all possible event types that can
 * occur in the system and be processed by the StateManager.
 */
enum EventType {
    // --- System / Null Event ---
    EVENT_NULL = 0,                 // Invalid or default event

    // --- Events from ConfigState ---
    EVENT_START_MANUAL_PLAY,        // User pressed "Start Manual" on web
    EVENT_START_AUTO_PLAY,          // User pressed "Start Auto" on web
    EVENT_START_CALIBRATION,        // User pressed "Start Calibration" on web

    // --- Events from Calibration State ---
    EVENT_CALIBRATION_COMPLETE,     // Calibration process completed successfully
    EVENT_CALIBRATION_CANCEL,       // Calibration process cancelled by user
    EVENT_CALIBRATION_FAILED,       // Calibration process failed (timeout or error)

    // --- Events from Play States ---
    EVENT_PLAY_FINISHED,            // User pressed "Finish" button (manual or auto)
    EVENT_TRAINING_SUCCESS,         // Trigger reward (e.g., dispense treat) success
    EVENT_TRAINING_FAILED,          // Trigger reward failed

    // --- Hardware Interrupt Events ---
    EVENT_MODE_ONLINE_ACTIVATED,    // Hardware switch moved to ONLINE
    EVENT_MODE_OFFLINE_ACTIVATED,   // Hardware switch moved to OFFLINE
    
    // --- Internal Module Events ---
    EVENT_SYNC_COMPLETED,           // SupabaseManager finished uploading
    EVENT_SYNC_FAILED               // SupabaseManager failed to upload
};

/**
 * @brief The data structure sent through the event queue.
 */
struct Event {
    EventType type;
    union {
        int     intValue;
        float   floatValue;
        bool    boolValue;
    } payload;
};


#endif // EVENTS_H
/****************************************************************
 * @file main.cpp
 * @brief LittleFS (File System) 
 * Testbench for writing, reading, appending, and deleting
 * files from the LittleFS partition.
 ****************************************************************/

/****************************************************************
 * Headers
 ****************************************************************/
#include <Arduino.h>
#include "FS.h"       // The base File System class
#include "LittleFS.h" // The LittleFS implementation

/****************************************************************
 * Defines
 ****************************************************************/
#define TEST_FILE_PATH "/test.txt"

/****************************************************************
 * Helper Function Prototypes
 ****************************************************************/

/**
 * @brief (Test 'w') Writes (overwrites) a file with new content.
 */
void writeFile(String path, String content);

/**
 * @brief (Test 'a') Appends new content to the end of a file.
 */
void appendFile(String path, String content);

/**
 * @brief (Test 'r') Reads the full content of a file.
 */
void readFile(String path);

/**
 * @brief (Test 'd') Deletes a file.
 */
void deleteFile(String path);

/**
 * @brief (Test 'f') Formats the entire filesystem. (DANGEROUS!)
 */
void formatFilesystem();

/****************************************************************
 * Setup Function
 ****************************************************************/
void setup() {
    Serial.begin(115200);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.println("\n--- LittleFS (File System) Sandbox ---");

    // 1. Mount the filesystem
    // .begin(true) -> format if mount fails
    // .begin(false) -> don't format (safer)
    if (!LittleFS.begin()) {
        Serial.println("Failed to mount LittleFS. Filesystem may be corrupt.");
        Serial.println("Try sending 'f' to format.");
    } else {
        Serial.println("LittleFS mounted successfully.");
    }

    Serial.println("\nSend commands via Serial Monitor (No new line/CR):");
    Serial.println(" 'w' -> Write (overwrite) test file");
    Serial.println(" 'a' -> Append to test file");
    Serial.println(" 'r' -> Read test file");
    Serial.println(" 'd' -> Delete test file");
    Serial.println(" 'f' -> Format *ENTIRE* filesystem (USE WITH CAUTION!)");

    Serial.println("\nInitial content of " + String(TEST_FILE_PATH) + ":");
    readFile(TEST_FILE_PATH);
}

/****************************************************************
 * Loop Function (Test Trigger)
 ****************************************************************/
void loop() {
    if (Serial.available() > 0) {
        char command = Serial.read();

        if (command == 'w') {
            Serial.println("\n[Test 'w'] Writing (overwriting) " + String(TEST_FILE_PATH) + "...");
            writeFile(TEST_FILE_PATH, "Hello, this is the first line.\n");
            Serial.println("Done. Reading file back:");
            readFile(TEST_FILE_PATH);
        
        } else if (command == 'a') {
            Serial.println("\n[Test 'a'] Appending to " + String(TEST_FILE_PATH) + "...");
            appendFile(TEST_FILE_PATH, "This is the second line.\n");
            Serial.println("Done. Reading file back:");
            readFile(TEST_FILE_PATH);

        } else if (command == 'r') {
            Serial.println("\n[Test 'r'] Reading " + String(TEST_FILE_PATH) + "...");
            readFile(TEST_FILE_PATH);

        } else if (command == 'd') {
            Serial.println("\n[Test 'd'] Deleting " + String(TEST_FILE_PATH) + "...");
            deleteFile(TEST_FILE_PATH);
            Serial.println("Done. Reading file back:");
            readFile(TEST_FILE_PATH);
        
        } else if (command == 'f') {
            Serial.println("\n[Test 'f'] WARNING: Formatting filesystem...");
            formatFilesystem();
            Serial.println("Done.");
        }
    }
    
    vTaskDelay(100 / portTICK_PERIOD_MS);
}

/****************************************************************
 * Helper Function Implementations
 ****************************************************************/

void writeFile(String path, String content) {
    // "w" = Write mode.
    // This *overwrites* the file if it exists, or creates it if it doesn't.
    File file = LittleFS.open(path, "w");
    if (!file) {
        Serial.println("Failed to open file for writing.");
        return;
    }
    
    if (file.print(content)) {
        Serial.println("File written successfully.");
    } else {
        Serial.println("Error writing to file.");
    }
    file.close(); 
}

void appendFile(String path, String content) {
    // "a" = Append mode.
    // This *adds* to the end of the file, or creates it if it doesn't.
    // ¡Esta es la función que usarás para tu log de sesiones!
    File file = LittleFS.open(path, "a");
    if (!file) {
        Serial.println("Failed to open file for appending.");
        return;
    }
    
    if (file.print(content)) {
        Serial.println("Content appended successfully.");
    } else {
        Serial.println("Error appending to file.");
    }
    file.close();
}

void readFile(String path) {
    // "r" = Read mode.
    File file = LittleFS.open(path, "r");
    if (!file) {
        Serial.println("Failed to open file for reading (it may not exist).");
        return;
    }

    Serial.println("--- Reading File: " + path + " ---");
    while (file.available()) {
        // Lee el archivo caracter por caracter y lo imprime
        Serial.write(file.read());
    }
    Serial.println("--- End of File ---");
    file.close();
}

void deleteFile(String path) {
    if (LittleFS.remove(path)) {
        Serial.println("File deleted successfully.");
    } else {
        Serial.println("Error deleting file (it may not exist).");
    }
}

void formatFilesystem() {
    Serial.println("Formatting... this may take a moment.");
    if (LittleFS.format()) {
        Serial.println("Filesystem formatted successfully.");
    } else {
        Serial.println("Error formatting filesystem.");
    }
}
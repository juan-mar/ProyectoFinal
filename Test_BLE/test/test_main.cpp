#include <Arduino.h>

// Asegúrate que estos sean tus pines correctos
#define RXD2 16
#define TXD2 17

void setup() {
  Serial.begin(115200);
  // Prueba 9600. Si salen garabatos, cambia a 115200
  // Algunos HM-10 clones vienen a 9600, otros a 115200.
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2); 
  
  Serial.println("--- MODO PUENTE (BUFFERED) ---");
  Serial.println("Escribe el comando completo (ej: AT) y dale Enter.");
}

void loop() {
  // 1. ESCUCHAR AL PC (Tu teclado)
  if (Serial.available()) {
    // Leemos toda la frase hasta que detectamos el "Enter" (\n)
    String comando = Serial.readStringUntil('\n');
    
    // Limpiamos espacios en blanco extra al final (trim)
    comando.trim(); 
    
    // Si escribiste algo válido, lo mandamos al módulo
    if (comando.length() > 0) {
      Serial.print("Enviando al modulo: ");
      Serial.println(comando);
      
      Serial2.print(comando); // Manda el comando limpio
      
      // OJO AQUÍ: La mayoría de los HM-10 NO quieren retorno de carro.
      // Pero si no te responde nada, descomenta las siguientes líneas una por una:
      // Serial2.write('\r'); 
      // Serial2.write('\n'); 
    }
  }

  // 2. ESCUCHAR AL MÓDULO (Su respuesta)
  if (Serial2.available()) {
    // Leemos lo que responde el módulo y lo mostramos
    char c = Serial2.read();
    Serial.write(c);
  }
}
#include <Arduino.h>

// Paso 1: Definir los estados de nuestro sistema
enum EstadoDelSistema {
  CONFIGURANDO,
  JUEGO_MANUAL,
  JUEGO_AUTOMATICO,
  SINCRONIZANDO
};

// Paso 2: Definir los eventos que pueden ocurrir
enum Evento {
  EVENTO_EMPEZAR_MANUAL,
  EVENTO_EMPEZAR_AUTOMATICO,
  EVENTO_FIN_JUEGO,
  EVENTO_INTERRUPTOR_A_ONLINE
};

// Variable para guardar el estado actual. 'volatile' es importante si una ISR la modifica.
volatile EstadoDelSistema estadoActual = CONFIGURANDO;

// Handle para la cola de eventos
QueueHandle_t colaDeEventos;

// Declaración de la función de la tarea para que setup() la conozca
void orquestadorTask(void *parameter);


void setup() {
  Serial.begin(115200);
  Serial.println("--- Sistema de Recompensas Iniciado ---");

  // Paso 3: Crear la cola. Puede almacenar hasta 10 eventos de tipo 'Evento'.
  colaDeEventos = xQueueCreate(10, sizeof(Evento));

  if (colaDeEventos == NULL) {
    Serial.println("Error creando la cola de eventos!");
    while(1); // Detener si hay un error crítico
  }

  // Paso 4: Crear la tarea del orquestador
  xTaskCreate(
      orquestadorTask,    // Función que implementa la tarea
      "Orquestador",      // Nombre de la tarea (para depuración)
      4096,               // Tamaño de la pila en palabras (4KB)
      NULL,               // Parámetros de la tarea (ninguno)
      1,                  // Prioridad (1 es una prioridad baja)
      NULL                // Handle de la tarea (no lo necesitamos)
  );

  Serial.println("Orquestador iniciado. Estado actual: CONFIGURANDO");
  Serial.println("Escribe comandos: 'empezar_manual', 'empezar_auto', 'fin_juego', 'sincronizar'");
}


// La tarea principal que gestiona la máquina de estados
void orquestadorTask(void *parameter) {
  Evento eventoRecibido;

  while (true) {
    // Paso 5: Esperar (bloquearse) hasta que llegue un evento a la cola.
    // El 'portMAX_DELAY' hace que espere indefinidamente.
    if (xQueueReceive(colaDeEventos, &eventoRecibido, portMAX_DELAY)) {
      
      Serial.printf("\n[Orquestador] Evento recibido: %d\n", eventoRecibido);

      // Lógica de transición basada en el estado actual Y el evento recibido
      switch (estadoActual) {
        
        case CONFIGURANDO:
          if (eventoRecibido == EVENTO_EMPEZAR_MANUAL) {
            estadoActual = JUEGO_MANUAL;
            Serial.println("[Orquestador] Transición a -> JUEGO_MANUAL");
            // Aquí llamarías a: manualController.iniciar();
            // Y a: webServerManager.detener();
          } 
          else if (eventoRecibido == EVENTO_EMPEZAR_AUTOMATICO) {
            estadoActual = JUEGO_AUTOMATICO;
            Serial.println("[Orquestador] Transición a -> JUEGO_AUTOMATICO");
            // Aquí llamarías a: autoController.iniciar();
          }
          else if (eventoRecibido == EVENTO_INTERRUPTOR_A_ONLINE) {
            estadoActual = SINCRONIZANDO;
            Serial.println("[Orquestador] Transición a -> SINCRONIZANDO");
            // Aquí llamarías a: supabaseManager.iniciarSincronizacion();
          }
          break;

        case JUEGO_MANUAL:
        case JUEGO_AUTOMATICO:
          if (eventoRecibido == EVENTO_FIN_JUEGO) {
            estadoActual = CONFIGURANDO;
            Serial.println("[Orquestador] Transición a -> CONFIGURANDO");
            // Aquí llamarías a: manualController.detener() o autoController.detener();
            // Y a: webServerManager.iniciar();
          }
          break;

        case SINCRONIZANDO:
          // En este ejemplo, la sincronización es instantánea.
          // En la vida real, otra tarea haría el trabajo y al final
          // enviaría un evento "EVENTO_SINC_TERMINADA".
          Serial.println("[Orquestador] Sincronización finalizada (simulado). Volviendo a CONFIGURANDO.");
          estadoActual = CONFIGURANDO;
          break;
      }
    }
  }
}

// El loop() ahora solo simula la llegada de eventos desde el exterior
void loop() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    Evento eventoAEnviar;
    bool comandoValido = true;

    if (command == "empezar_manual") {
      eventoAEnviar = EVENTO_EMPEZAR_MANUAL;
    } else if (command == "empezar_auto") {
      eventoAEnviar = EVENTO_EMPEZAR_AUTOMATICO;
    } else if (command == "fin_juego") {
      eventoAEnviar = EVENTO_FIN_JUEGO;
    } else if (command == "sincronizar") {
      eventoAEnviar = EVENTO_INTERRUPTOR_A_ONLINE;
    } else {
      comandoValido = false;
      Serial.println("Comando no reconocido.");
    }

    if (comandoValido) {
      Serial.printf("Enviando evento a la cola: %d\n", eventoAEnviar);
      // Enviamos el evento a la cola del orquestador
      xQueueSend(colaDeEventos, &eventoAEnviar, portMAX_DELAY);
    }
  }
  
  //blink bloqueante
  digitalWrite(2, HIGH);   // Encender el LED
  delay(100);                       // Esperar 100 ms
  digitalWrite(2, LOW);    // Apagar el LED 

  // El loop puede hacer otras cosas no críticas aquí, como leer un sensor
  // que no necesita una respuesta inmediata.
  //vTaskDelay(100 / portTICK_PERIOD_MS); // Pequeña pausa
}
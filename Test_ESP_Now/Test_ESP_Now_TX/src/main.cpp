#include <Arduino.h>
/*********
  Marce Ferra 2021
  Proyecto de emisor y receptor para tres entradas lógicas (para contacto seco) en emisor y tres salidas lógicas de estado en receptor
  Usando protocolo ESP NOW de Espressif
  MODULO EMISOR con BMP280 por I2C
  Si esta información te resulta útil e interesante, invitame un cafecito!!!
  https://cafecito.app/marce_ferra

  Desde fuera de Argentina en:
  https://www.buymeacoffee.com/marceferra

  If you found this information useful and interesting, buy me a cafecito!!!
  https://www.buymeacoffee.com/marceferra
*********/

#include <esp_now.h>
#include <WiFi.h>
#include <esp_sleep.h>

// Dirección MAC del receptor
uint8_t broadcastAddress[] = {0x0C, 0xB8, 0x15, 0xF5, 0x21, 0xE4};

typedef struct struct_message {
  char word[12];
} struct_message;

struct_message paquete_datos;

esp_now_peer_info_t peerInfo;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

void setup() {
  Serial.begin(115200);
  pinMode(5, OUTPUT);
//  digitalWrite(5, LOW);


  // Inicializa WiFi en modo estación
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(); // No queremos conectarnos a ninguna red

  // Inicializa ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error inicializando ESP-NOW");
    return;
  }

  // Registra el callback de envío
  esp_now_register_send_cb(OnDataSent);

  // Configura el peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0; // 0 si no conocés el canal exacto
  peerInfo.encrypt = false;

  // Agrega el peer
  if (!esp_now_is_peer_exist(broadcastAddress)) {
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("No se pudo agregar el peer");
      return;
    }
  }

  // Prepara el mensaje
  strcpy(paquete_datos.word, "hello world");

  // Envía el mensaje
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)&paquete_datos, sizeof(paquete_datos));
  if (result == ESP_OK) {
    Serial.println("Mensaje enviado");
    //digitalWrite(5, !digitalRead(5));
    //digitalWrite(5, !digitalRead(5)); // Cambia el estado del pin 5
  } else {
    Serial.println("Error al enviar");
  }

  delay(50); // Da tiempo a que se complete la transmisión
  //digitalWrite(5, LOW);

  // Configura el wake-up cada 0,1 segundo
  esp_sleep_enable_timer_wakeup(1 * 100000);

  //Serial.println("Entrando en deep sleep...");
  esp_deep_sleep_start();
}

void loop() {
  // No se usa, ya que el ESP se duerme en setup()
}

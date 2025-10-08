// MiClase.h
#ifndef TAG_H
#define TAG_H

#include <Arduino.h>
#include <Preferences.h>
#include "esp_wifi.h"
#include <esp_now.h>
#include <WiFi.h>
#include <SPI.h>
#include <Wire.h>

#define SAMPLE_INTERVAL 100
#define CAL_TIME 5000

// Variables globales
static Preferences prefs;

/******** Estructuras y Funciones para ESP-NOW ********/

typedef struct {
    char word[12];  
} struct_message_t;


typedef struct {
  unsigned frame_ctrl: 16;
  unsigned duration_id: 16;
  uint8_t addr1[6]; /* receiver address */
  uint8_t addr2[6]; /* sender address */
  uint8_t addr3[6]; /* filtering address */
  unsigned sequence_ctrl: 16;
  uint8_t addr4[6]; /* optional */
} wifi_ieee80211_mac_hdr_t;

typedef struct {
  wifi_ieee80211_mac_hdr_t hdr;
  uint8_t payload[0]; /* network data ended with 4 bytes csum (CRC32) */
} wifi_ieee80211_packet_t;


//Funciones para ESP-NOW
//La callback que hace la magia
void promiscuous_rx_cb(void *buf, wifi_promiscuous_pkt_type_t type);

// callback function that will be executed when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len);


/******** Fin estructuras y funciones para ESP-NOW ********/


// Clase Receptor
class Receptor {
  private:
    float threshold;
    float varianza;

    //int macAddr;

    //Declarancion de funciones privadas
    //Filtro de Kalman
    //float filtrado(float nuevaMuestra, float R = 0.1, float Q = 0.001);

  public:
    Receptor(); // Constructor
    ~Receptor(); // Destructor

    bool calibracion();
    int getRSSI();
    float getThreshold();
    float getVarianza();

};


// Clase Filtro
class Filtro {
  private:
    float x_est;
    float P_est;
    float varianzaR;
    float varianzaQ;
    
    //int macAddr;

    //Declarancion de funciones privadas
    //Filtro de Kalman
    //float filtroKalman(float nuevaMuestra, float R = 0.1, float Q = 0.001);
    float filtroKalman(float nuevaMuestra);

  public:
    Filtro(); // Constructor
    ~Filtro(); // Destructor

    void set_varianzaR(float var);
    void set_varianzaQ(float var);
    
    void filtrado(float val);

};

#endif

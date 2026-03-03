//********************************************************
//              Lanzador con NRF24 (Servidor)
//********************************************************

#include <SPI.h>
#include <RH_NRF24.h>

//Constantes
#define KBPS_250 (RH_NRF24::DataRate250kbps)
#define MBPS_1   (RH_NRF24::DataRate1Mbps)
#define MBPS_2   (RH_NRF24::DataRate2Mbps)
#define PW_0DB   (RH_NRF24::TransmitPower0dBm)
#define MAX_MSG_LEN (RH_NRF24_MAX_MESSAGE_LEN)

RH_NRF24 nrf24;

uint32_t cont = 0;

//Setup
void setup() 
{
  Serial.begin(9600);

  //Init NRF24
  if (!nrf24.init())
    Serial.println("NRF24 Init failed...");
  if (!nrf24.setChannel(2))
    Serial.println("Set Channel failed...");
  if (!nrf24.setRF(KBPS_250, PW_0DB))
    Serial.println("NRF24 Settings failed...");    

    Serial.println("Lanzador inicializado y listo");
}

//Loop
void loop()
{
  if (nrf24.available())
  { 
    uint8_t buf[MAX_MSG_LEN];
    uint8_t len = sizeof(buf);

    //Waits for message
    if (nrf24.recv(buf, &len))
    {
      cont++;
      Serial.print("Recibido Nº");
      Serial.print(cont);
      Serial.print(": ");
      Serial.println((char*)buf);
    }
    else
    {
      Serial.println("Received failed...");
    }
  }
}



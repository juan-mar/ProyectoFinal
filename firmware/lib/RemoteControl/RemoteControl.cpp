#include "RemoteControl.h"
#include "HardwareConfig.h"
#include "Config.h"

RemoteControl::RemoteControl() : nrf24(PIN_NRF24_CE, PIN_NRF24_CSN) {}

bool RemoteControl::init() {
    if (!nrf24.init()) return false;
    if (!nrf24.setChannel(2)) return false;
    if (!nrf24.setRF(RH_NRF24::DataRate250kbps, RH_NRF24::TransmitPower0dBm)) return false;
    nrf24.sleep();
    return true;
}

int RemoteControl::checkForCommand() {
    if (nrf24.available()) { 
        uint8_t buf[RH_NRF24_MAX_MESSAGE_LEN];
        uint8_t len = sizeof(buf);

        if (nrf24.recv(buf, &len)) {
            unsigned long receiveTime = millis();
            
            // Transformamos el buffer a un String para compararlo fácil
            String msg = String((char*)buf);
            msg.trim(); // Limpiamos basura o saltos de línea

            LOG_PRINTF("[RC][t=%lu] NRF24 mensaje recibido: '%s' (len=%d)\n", 
                       receiveTime, msg.c_str(), len);

            // ACÁ DEPENDE DE QUÉ TEXTO MANDE TU CONTROL REMOTO (TX)
            // Asumo unos textos de ejemplo, cambialos por los tuyos:
            if (msg == "BTN1") {
                LOG_PRINTF("[RC][t=%lu] *** BTN1 DETECTADO (SUCCESS) ***\n", receiveTime);
                return CMD_REMOTE_SUCCESS;
            }
            if (msg == "BTN2") {
                LOG_PRINTF("[RC][t=%lu] *** BTN2 DETECTADO (FAIL) ***\n", receiveTime);
                return CMD_REMOTE_FAIL;
            }
            if (msg == "FIN") {
                LOG_PRINTF("[RC][t=%lu] *** FIN DETECTADO (EXIT) ***\n", receiveTime);
                return CMD_REMOTE_EXIT;
            }
            
            LOG_PRINTF("[RC][t=%lu] WARNING: Mensaje no reconocido: '%s'\n", 
                       receiveTime, msg.c_str());
        }
    }
    return CMD_REMOTE_NONE; // No hay mensajes o falló la lectura
}

void RemoteControl::sleep() {
    nrf24.sleep(); // Lo manda a dormir profundamente
}
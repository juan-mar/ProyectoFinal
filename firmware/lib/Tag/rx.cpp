#include "rx.h"
#include "Filtro.h"
#include "Config.h"


/****************************************** DEFINICIONES *********************************************/
#define CAL_SAMPLES 50
#define NOT_DETECTED 0
#define DETECTED 1

/*************************************** VARIABLES GLOBALES *****************************************/
static float samples[CAL_SAMPLES];
static bool in_thres = false;

static bool calibrating = false;

static long sumRSSI = 0;
static int sampleCount = 0;

bool calib = false;
bool detected = false;
unsigned long timerDetec;

// Instancia global del filtro
static Filtro filtro_kalman;  



/*********************************** CALLBACKS DE ESCANEO ***********************************************/

// CALLBACK 1: Detecta el dispositivo y actualiza la variable 
class FiltradoRapidoCallback: public BLEAdvertisedDeviceCallbacks {
    Receptor* _ref;
    String _macBuscada;
public:
    FiltradoRapidoCallback(Receptor* ref, String mac) {
        _ref = ref;
        _macBuscada = mac;
    }
    
    void onResult(BLEAdvertisedDevice* advertisedDevice) {     
        unsigned long now = millis();
        
        String mac = advertisedDevice->getAddress().toString().c_str();    
        mac.toUpperCase();
        
        if (mac == _macBuscada) {
          //LOG_PRINTLN("Dispositivo detectado");  
          _ref->setNewMsg(true);   
          //LOG_PRINTLN("New Msg: " + String(_ref->isNewMsg()));
            
          calibrating = true;
            
          _ref->_procesarDato(advertisedDevice->getRSSI());
        }
    }
};

/*************************** CLASE RECEPTOR **********************************/

//Constructor. Recibe la MAC address del transmisor
Receptor::Receptor(String targetMac) {
    threshold = 0;
    varianza = 0;
    barrier = 0;
    _targetMac = targetMac;
    _targetMac.toUpperCase();
    rssiActual = -100;            // Valor inicial lejos
    new_msg = false;
    state = 0;
    _prevStateDetected = NOT_DETECTED;
}


// Inicia los callbacks y el escaneo de dispositivos (propio de BLE)
void Receptor::init() {
  BLEDevice::init("");
    _pBLEScan = BLEDevice::getScan();       //Iniciamos el escaneo de dispositivos
    
    // El segundo parámetro 'true' significa: wantDuplicates. Sino se registra el primer valor y nada mas.
    // Guardamos el puntero para poder borrarlo luego
    if (_pCallbacks == nullptr) { 
      _pCallbacks = new FiltradoRapidoCallback(this, _targetMac);
      _pBLEScan->setAdvertisedDeviceCallbacks(_pCallbacks, true);
    } 
    
    // --- LÍNEAS PARA MÁXIMA VELOCIDAD ---
    // Configura la antena para escuchar casi el 100% del tiempo
    // Los valores están en incrementos de 0.625ms (100 = 62.5ms)
    _pBLEScan->setInterval(100); 
    _pBLEScan->setWindow(99);    // Ventana casi igual al intervalo (99% Duty Cycle)
    
    // Escaneo activo: El ESP32 le pide al dispositivo que le responda más rápido si lo escucha.
    _pBLEScan->setActiveScan(true);

    // Escaneo infinito
    _sistemaActivo = true; // Marcamos el sistema como activo
    _pBLEScan->start(0, nullptr, false);
	_prevStateDetected = NOT_DETECTED; // Inicialmente no detectado

}


// Procesa un nuevo dato RSSI recibido y lo manda a filtrar
void Receptor::_procesarDato(int rssi) {
    rssiActual = rssi; 
    rssi_filtered = filtro_kalman.filtrado(rssiActual);
}

bool Receptor::calibracion() {
  // Es momento de tomar una nueva muestra?
  if (calibrating && (sampleCount < CAL_SAMPLES))
  {
    calibrating = false; // Esperamos a procesar esta muestra antes de tomar otra
    
    int rssi_calib = this->rssiActual;
    LOG_PRINT("Muestra RSSI: ");
    LOG_PRINTLN(rssi_calib);

    sampleCount++;
    sumRSSI += rssi_calib;
    samples[sampleCount - 1] = rssi_calib;

    return true; // calibración en progreso
  }
  else if (sampleCount >= CAL_SAMPLES) {
    LOG_PRINTLN("Fin calibracion");
    LOG_PRINT("Sample Count: ");
    LOG_PRINTLN(sampleCount);

    if (sampleCount > 0) {
      // Calculamos el promedio (Threshold)
      this->threshold = (float)sumRSSI / sampleCount;

      // Reiniciar la variable acumuladora antes de usarla
      float suma_diferencias = 0; 

      // Sumamos los cuadrados de las diferencias
      for(int i = 0; i < sampleCount; i++) {
        float diferencia = samples[i] - this->threshold;
        suma_diferencias += (diferencia * diferencia);
      }

      // Calculamos la Varianza Muestral (División única al final)
      // Usamos (sampleCount - 1) para mayor precisión estadística.
      if (sampleCount > 1) {
          this->varianza = suma_diferencias / (sampleCount - 1);
      } else {
          this->varianza = 0; // Evitar división por cero si solo hay 1 muestra
      }

      filtro_kalman.set_varianzaR(this->varianza);

      // sqrt(varianza) es la Desviación Estándar (Sigma).
      barrier = threshold - 1 * sqrt(this->varianza); 

      LOG_PRINT("Threshold: "); LOG_PRINTLN(this->threshold);
      LOG_PRINT("Varianza: "); LOG_PRINTLN(this->varianza);
      LOG_PRINT("Barrier:"); LOG_PRINTLN(barrier);

      suma_diferencias = 0;
    }

    sumRSSI = 0;
    sampleCount = 0;
    return false; // calibración finalizada
  }
  else {
    return true; // calibración en progreso
  }
}

bool Receptor::detect_thres() {
  int rssi_curr = rssi_filtered;

  // señal dentro del umbral
  if (rssi_curr > this->threshold) { 
    in_thres = true;
    return true; 
  } 

  // señal muy lejos
  else if (rssi_curr < barrier) { 
    in_thres = false;
    return false; 
  } 

  // señal detectada con tolerancia
  else if (in_thres && rssi_curr > barrier) { 
    return true; 
  }

  // señal no detectada
  else {
    return false; 
  }
}

int Receptor::getRSSI() {
    return rssiActual;
}


void Receptor::setNewMsg(bool val) {
    new_msg = val;
}

bool Receptor::isNewMsg() {
  //LOG_PRINTLN("New Msg: " + String(new_msg));
  return new_msg;
}


void Receptor::stop() {
    if (!_sistemaActivo) return; // Si ya está apagado, no hacemos nada

    LOG_PRINTLN("Deteniendo Receptor BLE...");

    // 1. Detener el motor de escaneo
    if(_pBLEScan != nullptr) {
        _pBLEScan->stop(); 
        _pBLEScan->clearResults(); // Libera la memoria de los dispositivos encontrados
        
        // Desvinculamos el callback para evitar llamadas fantasma
        //_pBLEScan->setAdvertisedDeviceCallbacks(nullptr);
    }

    // 2. Liberar la memoria del objeto Callback
    /*
    if (_pCallbacks != nullptr) {
        delete _pCallbacks;
        _pCallbacks = nullptr;
    }
    */
    
    // 3. Bloquear reinicios futuros
    _sistemaActivo = false;

    LOG_PRINTLN("Receptor detenido");
}

int Receptor::scan() {

  // Leemos la variable global directamente
  	int lecturaRaw = this->getRSSI();

	if (this->isNewMsg() || calib) { // si hay datos disponibles en el puerto serie
		new_msg = false; // reseteamos la variable para esperar el próximo mensaje
		if (state == CALIBRATION_RX || calib) {          // si estamos en estado de calibracion
			return ((calib = this->calibracion())?CALIBRATING:CALIB_OK);        // llamar a la función calibracion
		}
		else if (state ==  DETECTION_RX) {               // si estamos en estado de deteccion
			if(detect_thres()){
				if(_prevStateDetected == DETECTED) {
					//Nada - esta adentro y antes tambien
					return IDLE;
				}
				else{
					//ENTRO PERRO - Avisamos a FSM
					_prevStateDetected = DETECTED;
					return DETECT_OK;
				} 
			}
			else{
				if(_prevStateDetected == DETECTED) {
					//SE FUE EL PERRO - return Detect_fail
					_prevStateDetected = NOT_DETECTED;
					return DETECT_FAIL;
				}
				else{
					//Nada - estaba afuera y sigue afuera
					return IDLE;
				}
			} 
		}    
    }
  	return IDLE; // Si no hay datos nuevos, retornamos IDLE
}

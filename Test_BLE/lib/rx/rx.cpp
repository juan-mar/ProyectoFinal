#include "rx.h"


/************************** DEFINICIONES ****************************/
#define CAL_SAMPLES 50

/*********************** VARIABLES GLOBALES *************************/
static float rssi_filtered;
static float samples[CAL_SAMPLES];
static bool in_thres = false;

static bool calibrating = false;
static bool detecting = false;
static unsigned long lastSampleTime = 0;


static long sumRSSI = 0;
static long sumSqRSSI = 0;
static int sampleCount = 0;
static float sum_var = 0;

const int umbral = -60;
int promedio = -100;

bool calib = false;
bool detected = false;
bool flag = false;
unsigned long calibStart;

/************************* CLASE FILTRO ******************************/
class Filtro {
  private:
    float x_est;
    float P_est;
    float varianzaR;
    float varianzaQ;

    //Funciones privadas
    float filtroKalman(float nuevaMuestra){
        float R = varianzaR;
        float Q = varianzaQ;
        
        float x_pred = x_est;
        float P_pred = P_est + Q;

        // Ganancia de Kalman
        float K = P_pred / (P_pred + R);

        // Actualización
        x_est = x_pred + K * (nuevaMuestra - x_pred);
        P_est = (1 - K) * P_pred;

        return x_est;
    }

  public:
    Filtro(){ // Constructor
        x_est = -50;
        P_est = 100;
        varianzaR = 0.1;
        varianzaQ = 0.5;
    } 

    ~Filtro(){
      // Destructor
    }

    void set_varianzaR(float var){
      varianzaR = var;
    }
    void set_varianzaQ(float var){
      varianzaQ = var;
    }
    
    void filtrado(float nuevaMuestra){
      rssi_filtered = filtroKalman(nuevaMuestra);
    }
};


static Filtro filtro_kalman;  // Instancia global del filtro



/************************** CALLBACKS DE ESCANEO ******************************/
// Puntero auxiliar para conectar los callbacks estáticos con el objeto
Receptor* globalReceptorRef = nullptr;

// CALLBACK 1: Detecta el dispositivo y actualiza la variable 
class FiltradoRapidoCallback: public BLEAdvertisedDeviceCallbacks {
    Receptor* _ref;
    String _macBuscada;
public:
    FiltradoRapidoCallback(Receptor* ref, String mac) {
        _ref = ref;
        _macBuscada = mac;
    }
    
    // 1. AGREGA EL ASTERISCO (*) AQUÍ:
    void onResult(BLEAdvertisedDevice* advertisedDevice) {     
        unsigned long now = millis();
        
        // 2. CAMBIA EL PUNTO (.) POR LA FLECHA (->) AQUÍ:
        String mac = advertisedDevice->getAddress().toString().c_str();    
        mac.toUpperCase();
        
        if (mac == _macBuscada) {
            //Serial.println("Dispositivo detectado");  
            _ref->setNewMsg(true);   
            
            calibrating = true;
            
            // 3. CAMBIA EL PUNTO (.) POR LA FLECHA (->) AQUÍ TAMBIÉN:
            _ref->_procesarDato(advertisedDevice->getRSSI());
        }
    }
};

// CALLBACK 2: Limpiador de Memoria. Limpiamos la memoria para que no se llene con los dispositivos que se van encontrando
void scanCompleteCB(BLEScanResults results) {
    if (globalReceptorRef != nullptr) {
        globalReceptorRef->_reiniciarEscaneo();
    }
}

/*************************** CLASE RECEPTOR **********************************/

//Constructor. Recibe la MAC address del transmisor
Receptor::Receptor(String targetMac) {
    threshold = 0;
    varianza = 0;
    barrier = 0;
    _targetMac = targetMac;
    _targetMac.toUpperCase();
    rssiActual = -100;            // Valor inicial lejos
    ultimaActualizacion = 0;
    _escaneando = false;
    globalReceptorRef = this;     // Enganchamos el puntero global
    new_msg = false;
    state = 0;
}


// Inicia los callbacks y el escaneo de dispositivos (propio de BLE)
void Receptor::init() {
  BLEDevice::init("");
    _pBLEScan = BLEDevice::getScan();       //Iniciamos el escaneo de dispositivos
    
    // El segundo parámetro 'true' significa: wantDuplicates. Sino se registra el primer valor y nada mas.
    // Guardamos el puntero para poder borrarlo luego
    _pCallbacks = new FiltradoRapidoCallback(this, _targetMac);
    _pBLEScan->setAdvertisedDeviceCallbacks(_pCallbacks, true);

    // --- AGREGA ESTAS LÍNEAS PARA MÁXIMA VELOCIDAD ---
    
    // Configura la antena para escuchar casi el 100% del tiempo
    // Los valores están en incrementos de 0.625ms (100 = 62.5ms)
    _pBLEScan->setInterval(100); 
    _pBLEScan->setWindow(99);    // Ventana casi igual al intervalo (99% Duty Cycle)
    
    // Escaneo activo: El ESP32 le pide amablemente al dispositivo 
    // que le responda más rápido si lo escucha.
    _pBLEScan->setActiveScan(true);

    // Escaneo infinito
    _sistemaActivo = true; // Marcamos el sistema como activo
    _pBLEScan->start(0, nullptr, false);
}


// Procesa un nuevo dato RSSI recibido y lo manda a filtrar
void Receptor::_procesarDato(int rssi) {
    rssiActual = rssi; 
    filtro_kalman.filtrado(rssiActual);
    ultimaActualizacion = millis();
    new_msg = false;
}

bool Receptor::calibracion() {
  //calibrating = true;

  // Es momento de tomar una nueva muestra?
  if (calibrating && (sampleCount < CAL_SAMPLES))
  {
    calibrating = false; // Esperamos a procesar esta muestra antes de tomar otra
    
    int rssi_calib = this->rssiActual;
    Serial.print("Muestra RSSI: ");
    Serial.println(rssi_calib);

    sampleCount++;
    sumRSSI += rssi_calib;
    samples[sampleCount - 1] = rssi_calib;

    
    return true;
  }
  else if (sampleCount >= CAL_SAMPLES) {
    Serial.println("Fin calibracion");
    Serial.print("Sample Count: ");
    Serial.println(sampleCount);
    //calibrating = false;

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

      Serial.print("Threshold: "); Serial.println(this->threshold);
      Serial.print("Varianza: "); Serial.println(this->varianza);
      Serial.print("Barrier:"); Serial.println(barrier);

      suma_diferencias = 0;
    }

    sumRSSI = 0;
    sampleCount = 0;
    return false; 
  }
  else {
    return true; // calibración en progreso
  }
}

bool Receptor::detect_thres() {
  detecting = true;
  //Serial.println("RSSI Actual: " + String(rssiActual) + " | RSSI Filtrado: " + String(rssi_filtered));
  int rssi_curr = rssi_filtered;
  if (rssi_curr > this->threshold) { // señal dentro del umbral
    in_thres = true;
    return true; 
  } 
  else if (rssi_curr < barrier) { // señal muy lejos
    in_thres = false;
    return false; 
    detecting = false;
  } 
  else if (in_thres && rssi_curr > barrier) { // señal detectada con tolerancia
    return true; 
  }
  else {
    return false; // señal no detectada
    detecting = false;
  }
}

void Receptor::_reiniciarEscaneo() {
    _escaneando = false; // El loop() se encarga de arrancar de nuevo
}

void Receptor::clear() {
    // Si el sistema se desactivó, salimos inmediatamente
    if (!_sistemaActivo) return; 

    if (millis() - _tiempoUltimoReinicio > 5000) {
        //Verificamos si el puntero sigue siendo válido
        if(_pBLEScan == nullptr) return; 

        _pBLEScan->stop();
        _pBLEScan->clearResults();
        _pBLEScan->start(0, nullptr, false);
        _tiempoUltimoReinicio = millis();
    }
}

int Receptor::getRSSI() {
    return rssiActual;
}

long Receptor::getUltimaActualizacion() {
    return ultimaActualizacion;
}

void Receptor::setNewMsg(bool val) {
    new_msg = val;
}

bool Receptor::isNewMsg() {
  return new_msg;
}


void Receptor::stop() {
    //if (!_sistemaActivo) return; // Si ya está apagado, no hacemos nada

    Serial.println("Deteniendo Receptor BLE...");

    // 1. Detener el motor de escaneo
    if(_pBLEScan != nullptr) {
        _pBLEScan->stop(); 
        _pBLEScan->clearResults(); // Libera la memoria de los dispositivos encontrados
        
        // Desvinculamos el callback para evitar llamadas fantasma
        _pBLEScan->setAdvertisedDeviceCallbacks(nullptr);
    }

    // 2. Liberar la memoria del objeto Callback
    if (_pCallbacks != nullptr) {
        delete _pCallbacks;
        _pCallbacks = nullptr;
    }

    // 3. Bloquear reinicios futuros
    _sistemaActivo = false;
    _escaneando = false;
    
    Serial.println("Receptor detenido y memoria liberada.");
}

void Receptor::scan(int state) {
  // Ciclo infinito que hace la limpieza de datos
  this->clear();

  // Leemos la variable global directamente
  int lecturaRaw = this->getRSSI();
  
  // Seguridad por si se apaga el dispositivo
  //if (millis() - scanner.getUltimaActualizacion() > 3500) {
  //    lecturaRaw = -100; // Si no hay datos en 3.5s, asumimos lejos
  //    Serial.println("--- Perdió señal ---");
  //}

  if (this->isNewMsg() || calib || flag) { // si hay datos disponibles en el puerto serie
    if (state == CALIBRATION_RX || calib) {          // si el carácter es 'c'
      calib = this->calibracion();        // llamar a la función calibracion
    }
    else if (state ==  DETECTION_RX || flag) {               // si el carácter es 'd' o ya se detecto la señal
      if(!flag) {
      calibStart = millis();
      }
      flag = true;
      unsigned long now = millis();
      detected = this->detect_thres(); // llamar a la función detect_thres
      if (detected && now - calibStart < THRES_TIME) {
        digitalWrite(2, HIGH); // Enciende el LED
      } 
      else if(detected && now - calibStart >= THRES_TIME) {
        Serial.println("Señal detectada!");
        digitalWrite(2, LOW); // Apaga el LED
        flag = false;
      }
      else {
        digitalWrite(2, LOW); // Apaga el LED
        calibStart = millis();
      }
    }
  }
}
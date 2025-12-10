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
// Puntero auxiliar para conectar los callbacks estáticos con tu objeto
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
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        static unsigned long calibStart = millis();
        unsigned long now = millis();
        String mac = advertisedDevice.getAddress().toString().c_str();
        mac.toUpperCase();
        if (mac == _macBuscada) {
            // Actualización directa a la variable
            _ref->setNewMsg(true);
            _ref->_procesarDato(advertisedDevice.getRSSI());
        }
    }
};

// CALLBACK 2: Limpiador de Memoria
void scanCompleteCB(BLEScanResults results) {
    if (globalReceptorRef != nullptr) {
        globalReceptorRef->_reiniciarEscaneo();
    }
}

/*************************** CLASE RECEPTOR **********************************/

Receptor::Receptor(String targetMac) {
    threshold = 0;
    varianza = 0;
    barrier = 0;
    _targetMac = targetMac;
    _targetMac.toUpperCase();
    rssiActual = -100; // Valor inicial "lejos"
    ultimaActualizacion = 0;
    _escaneando = false;
    globalReceptorRef = this; // Enganchamos el puntero global
    new_msg = false;
}


// Inicia los callbacks y el escaneo de dispositivos (propio de BLE)
void Receptor::init() {
  BLEDevice::init("");
    _pBLEScan = BLEDevice::getScan();
    
    // El segundo parámetro 'true' significa: wantDuplicates (Quiero duplicados).
    _pBLEScan->setAdvertisedDeviceCallbacks(new FiltradoRapidoCallback(this, _targetMac), true);

    // Escaneo infinito
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
  calibrating = true;
  static unsigned long calibStart = millis();
  unsigned long now = millis();

  // Es momento de tomar una nueva muestra?
  if (new_msg && (sampleCount < CAL_SAMPLES))
  {
    new_msg = false;
    lastSampleTime = now;

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
    calibrating = false;

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
      
      Serial.print("Threshold: "); Serial.println(this->threshold);
      Serial.print("Varianza: "); Serial.println(this->varianza);

      // sqrt(varianza) es la Desviación Estándar (Sigma).
      barrier = threshold - 2 * sqrt(this->varianza); 
    }
    return false; 
  }
  else {
    return true; // calibración en progreso
  }
}

bool Receptor::detect_thres() {
  detecting = true;
  int rssi_curr = rssi_filtered;
  if (new_msg) {
    Serial.print("RSSI actual: ");
    Serial.println(rssi_curr);
    Serial.print("Threshold: ");
    Serial.println(this->threshold);
    Serial.print("Barrier: ");
    Serial.println(barrier);
  }
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

void Receptor::loop() {
    // Si no está escaneando, arrancamos un nuevo ciclo.
    if (millis() - _tiempoUltimoReinicio > 5000) {
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
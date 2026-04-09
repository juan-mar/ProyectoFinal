#include "Filtro.h"

/************************* CLASE FILTRO - FUNCIONES PRIVADAS ******************************/
float Filtro::filtroKalman(float nuevaMuestra){
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


/************************* CLASE FILTRO - FUNCIONES PUBLICAS ******************************/

Filtro::Filtro(){ // Constructor
    x0_init = -100;
    p0_init = 100;
    x_est = x0_init;
    P_est = p0_init;
    varianzaR = 0.1;
    varianzaQ = 0.5;
} 

Filtro::~Filtro(){
    // Destructor
}

void Filtro::set_varianzaR(float var){
    varianzaR = var;
}
void Filtro::set_varianzaQ(float var){
    varianzaQ = var;
}

void Filtro::reset(float x0, float p0) {
    x0_init = x0;
    p0_init = p0;
    x_est = x0_init;
    P_est = p0_init;
}

float Filtro::filtrado(float nuevaMuestra){
    return filtroKalman(nuevaMuestra);
}

float Filtro::getVarianzaR() const {
    return varianzaR;
}

float Filtro::getVarianzaQ() const {
    return varianzaQ;
}

float Filtro::getX0() const {
    return x0_init;
}

float Filtro::getP0() const {
    return p0_init;
}

float Filtro::getXest() const {
    return x_est;
}

float Filtro::getPest() const {
    return P_est;
}



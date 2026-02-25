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
    x_est = -50;
    P_est = 100;
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

float Filtro::filtrado(float nuevaMuestra){
    return filtroKalman(nuevaMuestra);
}



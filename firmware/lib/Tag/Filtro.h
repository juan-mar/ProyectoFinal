#ifndef FILTRO_H
#define FILTRO_H

#include <Arduino.h>


class Filtro {
  private:
    float x_est;
    float P_est;
    float varianzaR;
    float varianzaQ;

    //Funciones privadas
    float filtroKalman(float nuevaMuestra);

  public:
    Filtro(); // Constructor

    ~Filtro(); // Destructor

    void set_varianzaR(float var);

    void set_varianzaQ(float var);
    
    float filtrado(float nuevaMuestra);
};


#endif
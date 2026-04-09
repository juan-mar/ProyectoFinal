#ifndef FILTRO_H
#define FILTRO_H

#include <Arduino.h>


class Filtro {
  private:
    float x_est;
    float P_est;
    float x0_init;
    float p0_init;
    float varianzaR;
    float varianzaQ;

    //Funciones privadas
    float filtroKalman(float nuevaMuestra);

  public:
    Filtro(); // Constructor

    ~Filtro(); // Destructor

    void set_varianzaR(float var);

    void set_varianzaQ(float var);

    void reset(float x0 = -100.0f, float p0 = 100.0f);
    
    float filtrado(float nuevaMuestra);

    float getVarianzaR() const;
    float getVarianzaQ() const;
    float getX0() const;
    float getP0() const;
    float getXest() const;
    float getPest() const;
};


#endif
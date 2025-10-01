// MiClase.h
#ifndef TAG_H
#define TAG_H

#include <Arduino.h>
#include <Preferences.h>

#define SAMPLE_INTERVAL 100
#define CAL_TIME 5000

static bool calOK;
static bool calibrating;
static Preferences prefs;


class tag {
  private:
    float threshold;
    float varianza;
    int macAddr;

  public:
    tag(); // Constructor
    ~tag(); // Destructor


    void calibracion();
    float getRSSI();
    float getThreshold();
    float getVarianza();
};

#endif

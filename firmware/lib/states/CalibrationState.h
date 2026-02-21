/****************************************************************
 * @file CalibrationState.h
 * @brief Declares the CalibrationState class (Calibration mode).
 ****************************************************************/

/****************************************************************
 * Include Guards
 ****************************************************************/
#ifndef CALIBRATION_STATE_H
#define CALIBRATION_STATE_H

/****************************************************************
 * Headers
 ****************************************************************/
#include "State.h"

/****************************************************************
 * Forward Declarations
 ****************************************************************/
class DataManager;
class HardwareManager;

/****************************************************************
 * Class Declarations
 ****************************************************************/

/**
 * @brief Active during device calibration process.
 * Manages calibration of sensors and hardware components.
 */
class CalibrationState : public State {
public:
    CalibrationState(DataManager* dataManager, HardwareManager* hardwareManager);

    virtual void enter(StateManager* manager) override;
    virtual void execute(StateManager* manager) override;
    virtual void exit(StateManager* manager) override;

protected:
    virtual void handleEvent(StateManager* manager, Event& event) override;
    virtual void update(StateManager* manager) override;

private:
    DataManager* dataManager;       
    HardwareManager* hardwareManager;
    unsigned long calibrationStartTime;  // Time when calibration started (ms)
};

#endif // CALIBRATION_STATE_H

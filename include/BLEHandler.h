#pragma once
#include "Config.h"
#include "Types.h"
#include <Arduino.h>

class BLEHandler {
public:
  BLEHandler();
  void begin();

  // PID params (existing)
  bool takePending(PIDParams &out);

  // Tank control
  bool takeTankControl(TankControl &out);

  // Control mode
  bool takeControlMode(ControlMode &out);

  // Emergency stop
  bool takeEmergencyStop(bool &out);
  void clearEmergencyStop();

  // Calibration trigger
  bool takeCalibrationTrigger(bool &out);
  void clearCalibrationTrigger();

  // Telemetry - send data to subscribed clients
  void sendTelemetry(const TelemetryData &data);

  // Check if any pending commands
  bool hasPendingCommands();

private:
  void setupBleServer();

  volatile bool _hasPendingPid;
  PIDParams _pendingPid;

  volatile bool _hasPendingTank;
  TankControl _pendingTank;

  volatile bool _hasPendingMode;
  ControlMode _pendingMode;

  volatile bool _emergencyStop;
  volatile bool _hasEmergencyStop;

  volatile bool _calibrationTrigger;
  volatile bool _hasCalibrationTrigger;

  portMUX_TYPE mux;
};

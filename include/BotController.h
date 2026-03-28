#pragma once
#include "PIDController.h"
#include "MotorDriver.h"
#include "Config.h"
#include "IMU.h"
#include "Types.h"
#include "BLEHandler.h"
#include "display.h"

class SerialBridge;

class BotController {
public:
  BotController();
  void begin();
  void update(float dt);
  void requestPidParams(const PIDParams &p);
  void printCurrentPid();
  
  // Test mode control
  void setTestMode(bool enabled);
  bool isTestMode() const { return testModeRuntime; }
  void runSelfChecks();

  // Control mode
  void setControlMode(ControlMode mode);
  ControlMode getControlMode() const { return controlMode; }

  // Emergency stop
  void emergencyStop();
  bool isEmergencyStopActive() const { return emergencyStopActive; }
  void clearEmergencyStop();

  // Tank control (for serial/BLE)
  void applyTankControl(const TankControl &tank);

  // Telemetry access
  TelemetryData getTelemetry() const;

  // Motor access for telemetry
  MotorDriver leftMotor;
  MotorDriver rightMotor;
  
  IMU &getIMU() { return imu; }

  float targetPitch = 0.0f;
  float targetRoll = 0.0f;

private:
  void loadStoredPid();
  void savePidToStorage(float kp, float ki, float kd);
  void applyPendingPid();
  void applyControlModeChange(ControlMode mode);
  void processBleCommands();
  void sendBleTelemetry();

  BLEHandler ble;
  portMUX_TYPE mux;

  volatile bool pendingPid;
  PIDParams pendingParams;
  float stepsPerDegree;
  PIDController rollPid;
  PIDController pitchPid;

  IMU imu;
  Display display;
  bool testModeRuntime;

  volatile ControlMode controlMode;
  volatile bool emergencyStopActive;

  static constexpr float MAX_TANK_SPEED = 1000.0f;
};

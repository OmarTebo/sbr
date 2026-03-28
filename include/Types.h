#pragma once
#include <Arduino.h>

struct PIDParams {
  float kp;
  float ki;
  float kd;
};

enum class ControlMode : uint8_t {
  AUTO = 0,   // Pure PID balance control (default)
  MANUAL = 1,  // Pure tank control (no PID)
  MIXED = 2   // Tank control + PID corrections
};

struct TankControl {
  float leftMotor;   // -100.0 to +100.0 percent
  float rightMotor;  // -100.0 to +100.0 percent
};

struct TelemetryData {
  float pitch;      // degrees
  float roll;       // degrees
  float yaw;        // degrees
  float leftSpeed;  // steps per second
  float rightSpeed; // steps per second
  ControlMode mode; // current control mode
  bool emergencyStop; // true if emergency stop is active
};

struct BLECommands {
  bool emergencyStop;     // true if emergency stop triggered
  bool calibrationTrigger; // true if calibration requested
};

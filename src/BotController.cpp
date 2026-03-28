#include "Config.h"
#include "HardwareMap.h"
#include "BotController.h"
#include "test_mode.h"
#include <Preferences.h>

BotController::BotController() : 
  leftMotor(
    SWAP_MOTORS ? RIGHT_STEP : LEFT_STEP,
    SWAP_MOTORS ? RIGHT_DIR : LEFT_DIR,
    SWAP_MOTORS ? RIGHT_EN : LEFT_EN
  ),
  rightMotor(
    SWAP_MOTORS ? LEFT_STEP : RIGHT_STEP,
    SWAP_MOTORS ? LEFT_DIR : RIGHT_DIR,
    SWAP_MOTORS ? LEFT_EN : RIGHT_EN
  ) {
  portMUX_INITIALIZE(&mux);
  pendingPid = false;
  stepsPerDegree = STEPS_PER_DEGREE;
  testModeRuntime = TEST_MODE_RUNTIME;
  controlMode = ControlMode::AUTO;
  emergencyStopActive = false;
}

void BotController::begin() {
  Serial.printf("BOOT_TAG: %s\n", BOOT_TAG);
  
  leftMotor.begin();
  rightMotor.begin();
  display.begin();
  
  if (!imu.begin()) {
    Serial.println("IMU init failed — attempting I2C recover + retry");
    IMU::i2cBusRecover(I2C_SDA_PIN, I2C_SCL_PIN);
    delay(50);
    if (!imu.begin()) {
      Serial.println("IMU init failed after recover. Continuing without IMU.");
    } else {
      Serial.println("IMU init succeeded after recover.");
    }
  }
#if BLE_ENABLED
  ble.begin();
#endif

  loadStoredPid();
  
  if (TEST_MODE_ENABLED || testModeRuntime) {
    Serial.println("TEST_MODE: ENABLED (motors disabled)");
  }

  Serial.printf("CONTROL_MODE: %d (0=AUTO, 1=MANUAL, 2=MIXED)\n", static_cast<uint8_t>(controlMode));
}

void BotController::update(float dt) {
  imu.update(dt);

#if BLE_ENABLED
  processBleCommands();
#endif

  float currentRoll = imu.getRoll();
  float currentPitch = imu.getPitch();
  float currentYaw = imu.getYaw();

  static unsigned long _lastTelemetryMs = 0;
  const unsigned long _telemetryIntervalMs = 20;
  unsigned long _nowMs = millis();
  if (_nowMs - _lastTelemetryMs >= _telemetryIntervalMs) {
    _lastTelemetryMs = _nowMs;
    Serial.printf("PITCH:%.2f ROLL:%.2f YAW:%.2f\n", currentPitch, currentRoll, currentYaw);
#if BLE_ENABLED
    sendBleTelemetry();
#endif
  }

  if (TEST_MODE_ENABLED || testModeRuntime || emergencyStopActive) {
    display.update();
    return;
  }

  float leftSpeed = 0.0f;
  float rightSpeed = 0.0f;

#if BLE_ENABLED
  if (controlMode == ControlMode::MANUAL) {
    // Pure tank control - PID not used
    // Speed is handled in processBleCommands() via applyTankControl()
  } else {
    // AUTO or MIXED: use PID
    float rollOutDegPerSec = rollPid.compute(targetRoll, currentRoll, dt);
    float rollStepsPerSec = rollOutDegPerSec * stepsPerDegree;
    
    float leftSign = LEFT_MOTOR_SIGN * (INVERT_LEFT_MOTOR ? -1.0f : 1.0f);
    float rightSign = RIGHT_MOTOR_SIGN * (INVERT_RIGHT_MOTOR ? -1.0f : 1.0f);
    
    leftSpeed = rollStepsPerSec * leftSign;
    rightSpeed = rollStepsPerSec * rightSign;
  }
#else
  // No BLE: pure PID control
  float rollOutDegPerSec = rollPid.compute(targetRoll, currentRoll, dt);
  float rollStepsPerSec = rollOutDegPerSec * stepsPerDegree;
  
  float leftSign = LEFT_MOTOR_SIGN * (INVERT_LEFT_MOTOR ? -1.0f : 1.0f);
  float rightSign = RIGHT_MOTOR_SIGN * (INVERT_RIGHT_MOTOR ? -1.0f : 1.0f);
  
  leftSpeed = rollStepsPerSec * leftSign;
  rightSpeed = rollStepsPerSec * rightSign;
#endif

  leftMotor.setSpeedStepsPerSec(leftSpeed);
  rightMotor.setSpeedStepsPerSec(rightSpeed);
  leftMotor.runSpeed();
  rightMotor.runSpeed();

  display.update();
}

void BotController::processBleCommands() {
  PIDParams p;
  if (ble.takePending(p)) {
    portENTER_CRITICAL(&mux);
    pendingParams = p;
    pendingPid = true;
    portEXIT_CRITICAL(&mux);
  }
  if (pendingPid) applyPendingPid();

  bool estop;
  if (ble.takeEmergencyStop(estop)) {
    emergencyStop();
  }

  ControlMode mode;
  if (ble.takeControlMode(mode)) {
    applyControlModeChange(mode);
  }

  TankControl tank;
  if (ble.takeTankControl(tank)) {
    applyTankControl(tank);
  }

  bool calib;
  if (ble.takeCalibrationTrigger(calib)) {
    Serial.println("BLE: Calibration triggered");
    imu.calibrateBlocking();
  }
}

void BotController::applyTankControl(const TankControl &tank) {
  if (emergencyStopActive) return;

  float leftSign = LEFT_MOTOR_SIGN * (INVERT_LEFT_MOTOR ? -1.0f : 1.0f);
  float rightSign = RIGHT_MOTOR_SIGN * (INVERT_RIGHT_MOTOR ? -1.0f : 1.0f);
  
  float leftSpeed = (tank.leftMotor / 100.0f) * MAX_TANK_SPEED * leftSign;
  float rightSpeed = (tank.rightMotor / 100.0f) * MAX_TANK_SPEED * rightSign;

  leftMotor.setSpeedStepsPerSec(leftSpeed);
  rightMotor.setSpeedStepsPerSec(rightSpeed);
  leftMotor.runSpeed();
  rightMotor.runSpeed();
}

void BotController::applyControlModeChange(ControlMode mode) {
  portENTER_CRITICAL(&mux);
  controlMode = mode;
  portEXIT_CRITICAL(&mux);
  
  const char* modeNames[] = {"AUTO", "MANUAL", "MIXED"};
  Serial.printf("BLE: Control mode changed to %s\n", modeNames[static_cast<uint8_t>(mode)]);
  
  if (mode == ControlMode::MANUAL) {
    rollPid.reset();
  }
}

void BotController::sendBleTelemetry() {
  TelemetryData data = getTelemetry();
  ble.sendTelemetry(data);
}

TelemetryData BotController::getTelemetry() const {
  TelemetryData data;
  data.pitch = imu.getPitch();
  data.roll = imu.getRoll();
  data.yaw = imu.getYaw();
  data.leftSpeed = 0; // Would need motor speed tracking
  data.rightSpeed = 0;
  data.mode = controlMode;
  data.emergencyStop = emergencyStopActive;
  return data;
}

void BotController::emergencyStop() {
  portENTER_CRITICAL(&mux);
  emergencyStopActive = true;
  portEXIT_CRITICAL(&mux);
  
  leftMotor.setSpeedStepsPerSec(0);
  rightMotor.setSpeedStepsPerSec(0);
  
  Serial.println("EMERGENCY STOP ACTIVATED");
}

void BotController::clearEmergencyStop() {
  portENTER_CRITICAL(&mux);
  emergencyStopActive = false;
  portEXIT_CRITICAL(&mux);
  
  ble.clearEmergencyStop();
  rollPid.reset();
  
  Serial.println("Emergency stop cleared");
}

void BotController::setControlMode(ControlMode mode) {
  applyControlModeChange(mode);
}

void BotController::requestPidParams(const PIDParams &p) {
  portENTER_CRITICAL(&mux);
  pendingParams = p;
  pendingPid = true;
  portEXIT_CRITICAL(&mux);
}

void BotController::applyPendingPid() {
  portENTER_CRITICAL(&mux);
  if (pendingPid) {
    rollPid.setTunings(pendingParams.kp, pendingParams.ki, pendingParams.kd);
    rollPid.reset();
    savePidToStorage(pendingParams.kp, pendingParams.ki, pendingParams.kd);
    pendingPid = false;
  }
  portEXIT_CRITICAL(&mux);
}

void BotController::printCurrentPid() {
  float kp, ki, kd;
  rollPid.getTunings(kp, ki, kd);
  Serial.printf("Roll PID - KP: %.6f KI: %.6f KD: %.6f\n", kp, ki, kd);
}

void BotController::loadStoredPid() {
  Preferences prefs;
  if (!prefs.begin(PREFS_NAMESPACE, false)) {
    Serial.println("Prefs begin failed - using defaults");
    rollPid.begin(DEFAULT_PID_KP, DEFAULT_PID_KI, DEFAULT_PID_KD, PID_OUTPUT_MIN_F, PID_OUTPUT_MAX_F);
    pitchPid.begin(DEFAULT_PID_KP, DEFAULT_PID_KI, DEFAULT_PID_KD, PID_OUTPUT_MIN_F, PID_OUTPUT_MAX_F);
    return;
  }

  float kp = prefs.getFloat(PREFS_KEY_KP, DEFAULT_PID_KP);
  float ki = prefs.getFloat(PREFS_KEY_KI, DEFAULT_PID_KI);
  float kd = prefs.getFloat(PREFS_KEY_KD, DEFAULT_PID_KD);

  prefs.end();

  Serial.printf("Loaded Roll PID from NVS: KP=%.6f KI=%.6f KD=%.6f\n", kp, ki, kd);
  rollPid.begin(kp, ki, kd, PID_OUTPUT_MIN_F, PID_OUTPUT_MAX_F);
  pitchPid.begin(kp, ki, kd, PID_OUTPUT_MIN_F, PID_OUTPUT_MAX_F);
}

void BotController::savePidToStorage(float kp, float ki, float kd) {
  Preferences prefs;
  if (!prefs.begin(PREFS_NAMESPACE, false)) {
    Serial.println("Prefs begin failed - cannot save PID");
    return;
  }
  prefs.putFloat(PREFS_KEY_KP, kp);
  prefs.putFloat(PREFS_KEY_KI, ki);
  prefs.putFloat(PREFS_KEY_KD, kd);
  prefs.end();
  Serial.printf("Saved Roll PID to NVS: KP=%.6f KI=%.6f KD=%.6f\n", kp, ki, kd);
}

void BotController::setTestMode(bool enabled) {
  testModeRuntime = enabled;
  if (enabled) {
    Serial.println("OK TEST_MODE: ON (motors disabled)");
  } else {
    Serial.println("OK TEST_MODE: OFF (motors enabled)");
  }
}

void BotController::runSelfChecks() {
  bool allPassed = ::runSelfChecks(imu, leftMotor, rightMotor, display, ble, rollPid);
}

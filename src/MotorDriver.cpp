#include "MotorDriver.h"
#include <Arduino.h>

// Constructor: initialize member stepper via initializer list
MotorDriver::MotorDriver(uint8_t stepPin, uint8_t dirPin, int8_t _enPin)
  : stepper(AccelStepper::DRIVER, stepPin, dirPin), enPin(_enPin) {
}

void MotorDriver::begin() {
  // handle enable pin manually (some AccelStepper forks may not provide setEnablePin)
  if (enPin >= 0) {
    pinMode(enPin, OUTPUT);
    // assume driver ENABLE is active LOW; set HIGH to disable initially
    digitalWrite(enPin, HIGH);
  }
  // Ensure stepper pins orientation default (no inversion)
  stepper.setPinsInverted(false, false, false);

  stepper.setAcceleration(MOTOR_DEFAULT_ACCELERATION);
  stepper.setMaxSpeed(MOTOR_DEFAULT_MAX_SPEED);
}

void MotorDriver::setSpeedStepsPerSec(float stepsPerSec) {
  // AccelStepper::setSpeed expects steps per second (positive/negative)
  stepper.setSpeed(stepsPerSec);
}

void MotorDriver::runSpeed() {
  // must call frequently (tight loop or timer)
  stepper.runSpeed();
}

void MotorDriver::enable(bool en) {
  if (enPin < 0) return;
  // assume active LOW enable
  digitalWrite(enPin, en ? LOW : HIGH);
}

void MotorDriver::setMaxSpeed(float s) {
  stepper.setMaxSpeed(s);
}

void MotorDriver::setAcceleration(float a) {
  stepper.setAcceleration(a);
}

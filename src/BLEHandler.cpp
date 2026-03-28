#include "BLEHandler.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// Service UUID
#define SERVICE_UUID "d1c6f3e0-9d3b-11ee-be56-0242ac120002"

// PID Characteristics
#define CHAR_KP_UUID "d1c6f3e1-9d3b-11ee-be56-0242ac120002"
#define CHAR_KI_UUID "d1c6f3e2-9d3b-11ee-be56-0242ac120002"
#define CHAR_KD_UUID "d1c6f3e3-9d3b-11ee-be56-0242ac120002"

// Tank Control Characteristics
#define CHAR_TANK_LEFT_UUID "d1c6f3e4-9d3b-11ee-be56-0242ac120002"
#define CHAR_TANK_RIGHT_UUID "d1c6f3e5-9d3b-11ee-be56-0242ac120002"

// Control Mode
#define CHAR_CONTROL_MODE_UUID "d1c6f3e6-9d3b-11ee-be56-0242ac120002"

// Emergency Stop
#define CHAR_EMERGENCY_STOP_UUID "d1c6f3e7-9d3b-11ee-be56-0242ac120002"

// Calibration Trigger
#define CHAR_CALIBRATE_UUID "d1c6f3e8-9d3b-11ee-be56-0242ac120002"

// Telemetry (Notify)
#define CHAR_TELEMETRY_UUID "d1c6f3e9-9d3b-11ee-be56-0242ac120002"

static BLECharacteristic *kpChar = nullptr;
static BLECharacteristic *kiChar = nullptr;
static BLECharacteristic *kdChar = nullptr;
static BLECharacteristic *tankLeftChar = nullptr;
static BLECharacteristic *tankRightChar = nullptr;
static BLECharacteristic *controlModeChar = nullptr;
static BLECharacteristic *emergencyStopChar = nullptr;
static BLECharacteristic *calibrateChar = nullptr;
static BLECharacteristic *telemetryChar = nullptr;

static BLEHandler *g_handler = nullptr;

BLEHandler::BLEHandler() {
  _hasPendingPid = false;
  _hasPendingTank = false;
  _hasPendingMode = false;
  _emergencyStop = false;
  _hasEmergencyStop = false;
  _calibrationTrigger = false;
  _hasCalibrationTrigger = false;
  _pendingPid = {0, 0, 0};
  _pendingTank = {0, 0};
  _pendingMode = ControlMode::AUTO;
  mux = portMUX_INITIALIZER_UNLOCKED;
}

void BLEHandler::begin() {
  g_handler = this;
  setupBleServer();
}

class BLEHandlerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    Serial.println("BLE: Client connected");
  }
  void onDisconnect(BLEServer* pServer) {
    Serial.println("BLE: Client disconnected");
  }
};

class WriteCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *chr) {
    if (!g_handler) return;
    portENTER_CRITICAL(&g_handler->mux);

    std::string v = chr->getValue();

    if (chr == kpChar) {
      g_handler->_pendingPid.kp = atof(v.c_str());
      g_handler->_hasPendingPid = true;
    } else if (chr == kiChar) {
      g_handler->_pendingPid.ki = atof(v.c_str());
      g_handler->_hasPendingPid = true;
    } else if (chr == kdChar) {
      g_handler->_pendingPid.kd = atof(v.c_str());
      g_handler->_hasPendingPid = true;
    } else if (chr == tankLeftChar) {
      g_handler->_pendingTank.leftMotor = atof(v.c_str());
      g_handler->_hasPendingTank = true;
    } else if (chr == tankRightChar) {
      g_handler->_pendingTank.rightMotor = atof(v.c_str());
      g_handler->_hasPendingTank = true;
    } else if (chr == controlModeChar) {
      uint8_t mode = v.length() > 0 ? v[0] : 0;
      if (mode > 2) mode = 0;
      g_handler->_pendingMode = static_cast<ControlMode>(mode);
      g_handler->_hasPendingMode = true;
    } else if (chr == emergencyStopChar) {
      uint8_t val = v.length() > 0 ? v[0] : 0;
      if (val != 0) {
        g_handler->_emergencyStop = true;
        g_handler->_hasEmergencyStop = true;
      }
    } else if (chr == calibrateChar) {
      uint8_t val = v.length() > 0 ? v[0] : 0;
      if (val != 0) {
        g_handler->_calibrationTrigger = true;
        g_handler->_hasCalibrationTrigger = true;
      }
    }

    portEXIT_CRITICAL(&g_handler->mux);
  }
};

void BLEHandler::setupBleServer() {
  BLEDevice::init("SBR-Bot");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new BLEHandlerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  kpChar = pService->createCharacteristic(CHAR_KP_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_READ);
  kiChar = pService->createCharacteristic(CHAR_KI_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_READ);
  kdChar = pService->createCharacteristic(CHAR_KD_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_READ);

  tankLeftChar = pService->createCharacteristic(CHAR_TANK_LEFT_UUID, BLECharacteristic::PROPERTY_WRITE);
  tankRightChar = pService->createCharacteristic(CHAR_TANK_RIGHT_UUID, BLECharacteristic::PROPERTY_WRITE);
  controlModeChar = pService->createCharacteristic(CHAR_CONTROL_MODE_UUID, BLECharacteristic::PROPERTY_WRITE);
  emergencyStopChar = pService->createCharacteristic(CHAR_EMERGENCY_STOP_UUID, BLECharacteristic::PROPERTY_WRITE);
  calibrateChar = pService->createCharacteristic(CHAR_CALIBRATE_UUID, BLECharacteristic::PROPERTY_WRITE);

  telemetryChar = pService->createCharacteristic(CHAR_TELEMETRY_UUID, BLECharacteristic::PROPERTY_NOTIFY);

  BLECharacteristicCallbacks *pCallback = new WriteCallback();
  kpChar->setCallbacks(pCallback);
  kiChar->setCallbacks(pCallback);
  kdChar->setCallbacks(pCallback);
  tankLeftChar->setCallbacks(pCallback);
  tankRightChar->setCallbacks(pCallback);
  controlModeChar->setCallbacks(pCallback);
  emergencyStopChar->setCallbacks(pCallback);
  calibrateChar->setCallbacks(pCallback);

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();

  Serial.println("BLE: Server started, advertising 'SBR-Bot'");
}

bool BLEHandler::takePending(PIDParams &out) {
  bool rv = false;
  portENTER_CRITICAL(&mux);
  if (_hasPendingPid) {
    out = _pendingPid;
    _hasPendingPid = false;
    rv = true;
  }
  portEXIT_CRITICAL(&mux);
  return rv;
}

bool BLEHandler::takeTankControl(TankControl &out) {
  bool rv = false;
  portENTER_CRITICAL(&mux);
  if (_hasPendingTank) {
    out = _pendingTank;
    _hasPendingTank = false;
    rv = true;
  }
  portEXIT_CRITICAL(&mux);
  return rv;
}

bool BLEHandler::takeControlMode(ControlMode &out) {
  bool rv = false;
  portENTER_CRITICAL(&mux);
  if (_hasPendingMode) {
    out = _pendingMode;
    _hasPendingMode = false;
    rv = true;
  }
  portEXIT_CRITICAL(&mux);
  return rv;
}

bool BLEHandler::takeEmergencyStop(bool &out) {
  bool rv = false;
  portENTER_CRITICAL(&mux);
  if (_hasEmergencyStop) {
    out = _emergencyStop;
    _hasEmergencyStop = false;
    rv = true;
  }
  portEXIT_CRITICAL(&mux);
  return rv;
}

void BLEHandler::clearEmergencyStop() {
  portENTER_CRITICAL(&mux);
  _emergencyStop = false;
  portEXIT_CRITICAL(&mux);
}

bool BLEHandler::takeCalibrationTrigger(bool &out) {
  bool rv = false;
  portENTER_CRITICAL(&mux);
  if (_hasCalibrationTrigger) {
    out = _calibrationTrigger;
    _hasCalibrationTrigger = false;
    rv = true;
  }
  portEXIT_CRITICAL(&mux);
  return rv;
}

void BLEHandler::clearCalibrationTrigger() {
  portENTER_CRITICAL(&mux);
  _calibrationTrigger = false;
  portEXIT_CRITICAL(&mux);
}

void BLEHandler::sendTelemetry(const TelemetryData &data) {
  if (!telemetryChar) return;

  char buf[64];
  int len = snprintf(buf, sizeof(buf), "%.2f,%.2f,%.2f,%.1f,%.1f,%d,%d",
    data.pitch, data.roll, data.yaw,
    data.leftSpeed, data.rightSpeed,
    static_cast<uint8_t>(data.mode),
    data.emergencyStop ? 1 : 0
  );

  telemetryChar->setValue((uint8_t*)buf, len);
  telemetryChar->notify();
}

bool BLEHandler::hasPendingCommands() {
  return _hasPendingPid || _hasPendingTank || _hasPendingMode || _hasEmergencyStop || _hasCalibrationTrigger;
}

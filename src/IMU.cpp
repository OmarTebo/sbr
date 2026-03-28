// src/IMU.cpp
// Application-level IMU wrapper for MPU6050.
// NOTE:
// - DLPF / sample-rate configuration is done here on purpose so that the
//   bundled MPU6050 library under lib/ remains untouched and reusable as-is.
// - Runtime DLPF changes are exposed via serial commands; BLE is kept separate.
// - To revert: remove/undo the configureDLPF/getDLPFConfig helpers and
//   associated SerialBridge branches; IMU library itself stays unchanged.
#include "IMU.h"
#include "Config.h"
#include <Wire.h>
#include <Arduino.h>
#include <math.h>
#include <Preferences.h>

#include "MPU6050.h"

// ---------------------------------------------------------------------------
// MPU6050 register-level configuration (DLPF + sample rate)
// These constants are local to this translation unit and intentionally
// not exposed in headers to keep the public API stable.
// Reference: MPU-6000/MPU-6050 Product Specification, Register Map.
// ---------------------------------------------------------------------------
constexpr uint8_t MPU_ADDR_DEFAULT     = 0x68;  // default I2C address
constexpr uint8_t MPU_ADDR_ALT         = 0x69;  // AD0 high
constexpr uint8_t REG_PWR_MGMT_1       = 0x6B;
constexpr uint8_t REG_SMPLRT_DIV       = 0x19;
constexpr uint8_t REG_CONFIG           = 0x1A;
constexpr uint8_t REG_GYRO_CONFIG      = 0x1B;
constexpr uint8_t CONFIG_DLPF_MASK     = 0x07;  // bits [2:0] DLPF_CFG
constexpr uint8_t CONFIG_KEEP_MASK     = 0xF8;  // keep bits [7:3] (EXT_SYNC_SET etc.)

// Recommended defaults for a 200 Hz control loop when DLPF is enabled:
// - Internal gyro output: 1 kHz when DLPF enabled
// - SampleRate = 1000 / (1 + SMPLRT_DIV)
//   => SMPLRT_DIV = 4 gives 1000 / (1 + 4) = 200 Hz
constexpr uint8_t DEFAULT_DLPF_CFG     = 1; // 188 Hz bandwidth, ~1.9 ms delay
constexpr uint8_t DEFAULT_SMPLRT_DIV   = 4; // 200 Hz sample rate
// After a DLPF configuration change, ignore a small number of samples to allow
// the on-chip filter to settle.
constexpr uint8_t DLPF_SETTLE_SAMPLES  = 2;

// Forward declarations so they can be used from IMU::begin() and SerialBridge
bool configureDLPF(uint8_t dlpf_cfg = DEFAULT_DLPF_CFG,
                   uint8_t smplrt_div = DEFAULT_SMPLRT_DIV,
                   bool force = false);
bool getDLPFConfig(uint8_t &dlpf_cfg, uint8_t &smplrt_div);
const char *getImuDlpfLastError();
bool saveImuDlpfConfigToNvs(uint8_t dlpf_cfg, uint8_t smplrt_div);
bool loadImuDlpfConfigFromNvs(uint8_t &dlpf_cfg, uint8_t &smplrt_div);

// Keep a handle to the active MPU instance and address so we can
// talk to the same device at the register level via Wire.
static MPU6050 *mpu = nullptr;
static uint8_t g_mpuAddr = MPU_ADDR_DEFAULT;
static uint8_t g_dlpfSettleRemaining = 0;

// Last error string for DLPF-related operations; retrievable via serial.
static String g_lastDlpfError;

static void setDlpfError(const char *msg) {
  if (msg) {
    g_lastDlpfError = msg;
  } else {
    g_lastDlpfError = "";
  }
}

const char *getImuDlpfLastError() {
  if (g_lastDlpfError.length() == 0) {
    return "NONE";
  }
  return g_lastDlpfError.c_str();
}

// Low-level helpers using Arduino Wire. We intentionally do not touch the
// internal MPU6050_Driver::writeReg/readRegs APIs to avoid changing the
// library surface. If the library later exposes public register helpers,
// these wrappers can be updated to prefer those.
static bool writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(g_mpuAddr);
  if (Wire.write(reg) != 1 || Wire.write(val) != 1) {
    Wire.endTransmission(); // best-effort cleanup
    Serial.println("IMU DLPF: writeReg() write failed");
    return false;
  }
  uint8_t status = Wire.endTransmission();
  if (status != 0) {
    Serial.printf("IMU DLPF: writeReg(0x%02X)=0x%02X failed, status=%u\n",
                  reg, val, status);
    return false;
  }
  return true;
}

static bool readReg(uint8_t reg, uint8_t &out) {
  Wire.beginTransmission(g_mpuAddr);
  if (Wire.write(reg) != 1) {
    Wire.endTransmission();
    Serial.printf("IMU DLPF: readReg() addr write failed (reg=0x%02X)\n", reg);
    return false;
  }
  uint8_t status = Wire.endTransmission(false); // repeated START
  if (status != 0) {
    Serial.printf("IMU DLPF: readReg() addr phase failed, status=%u\n", status);
    return false;
  }

  uint8_t n = Wire.requestFrom(static_cast<int>(g_mpuAddr), 1);
  if (n != 1 || !Wire.available()) {
    Serial.printf("IMU DLPF: readReg() data phase failed (reg=0x%02X, n=%u)\n",
                  reg, n);
    return false;
  }
  out = Wire.read();
  return true;
}

// Configure MPU6050 Digital Low-Pass Filter (DLPF) and sample rate for a
// 200 Hz control loop without modifying the underlying library. This function:
//  - Optionally checks that motors are not enabled (placeholder for now)
//  - Wakes the device (PWR_MGMT_1 = 0x00)
//  - Performs read-modify-write on CONFIG (0x1A) to set DLPF_CFG bits [2:0]
//  - Sets SMPLRT_DIV (0x19) so SampleRate = 1000 / (1 + SMPLRT_DIV)
//  - Reads back CONFIG and SMPLRT_DIV and logs the values for verification
//
// Returns true on successful configuration and verification.
// Safety: by default we should refuse to apply while motors are enabled.
// There is currently no direct BotController::motorsEnabled() accessor here,
// so we implement a placeholder and rely on FORCE usage from the serial
// command layer when tuning at runtime.
static bool areMotorsEnabledPlaceholder() {
  // TODO: adapt to actual BotController API when available
  return false;
}

// Optional helper: map DLPF_CFG to an approximate group delay (ms).
static const char *dlpfDelayMsString(uint8_t cfg) {
  switch (cfg & 0x07) {
    case 0: return "0.98";  // ~256 Hz (approx values from datasheet)
    case 1: return "1.90";  // 188 Hz
    case 2: return "2.80";  // 98 Hz
    case 3: return "4.90";  // 42 Hz
    case 4: return "8.30";  // 20 Hz
    case 5: return "13.40"; // 10 Hz
    case 6: return "18.60"; // 5 Hz
    case 7: return "approx"; // reserved / implementation-defined
    default: return "approx";
  }
}

bool configureDLPF(uint8_t dlpf_cfg, uint8_t smplrt_div, bool force) {
  uint8_t originalConfig = 0;

  setDlpfError(nullptr);

  if (!force && areMotorsEnabledPlaceholder()) {
    setDlpfError("MOTORS_ENABLED");
    Serial.println("IMU DLPF: motors enabled - refusing config without FORCE");
    return false;
  }

  Serial.printf("IMU: configuring MPU6050 DLPF (addr=0x%02X, cfg=%u, div=%u)\n",
                g_mpuAddr, dlpf_cfg, smplrt_div);

  // Wake device: clear SLEEP bit in PWR_MGMT_1
  if (!writeReg(REG_PWR_MGMT_1, 0x00)) {
    setDlpfError("I2C_WAKE_FAILED");
    Serial.println("IMU DLPF: failed to wake device (PWR_MGMT_1)");
    return false;
  }

  // Read CONFIG so we can preserve EXT_SYNC_SET and other upper bits
  if (!readReg(REG_CONFIG, originalConfig)) {
    setDlpfError("I2C_READ_CONFIG_FAILED");
    Serial.println("IMU DLPF: failed to read CONFIG");
    return false;
  }

  uint8_t newConfig = (originalConfig & CONFIG_KEEP_MASK) |
                      (dlpf_cfg & CONFIG_DLPF_MASK);
  if (!writeReg(REG_CONFIG, newConfig)) {
    setDlpfError("I2C_WRITE_CONFIG_FAILED");
    Serial.println("IMU DLPF: failed to write CONFIG");
    return false;
  }

  // Set sample rate divider for desired output rate
  if (!writeReg(REG_SMPLRT_DIV, smplrt_div)) {
    setDlpfError("I2C_WRITE_SMPLRT_FAILED");
    Serial.println("IMU DLPF: failed to write SMPLRT_DIV");
    return false;
  }

  // Optional: read GYRO_CONFIG for logging (no modifications here)
  uint8_t gyroCfg = 0;
  if (!readReg(REG_GYRO_CONFIG, gyroCfg)) {
    Serial.println("IMU DLPF: warning - failed to read GYRO_CONFIG for logging");
  }

  // Read back for verification
  uint8_t verifyConfig = 0;
  uint8_t verifyDiv    = 0;
  bool okCfg = readReg(REG_CONFIG, verifyConfig);
  bool okDiv = readReg(REG_SMPLRT_DIV, verifyDiv);

  Serial.printf(
      "IMU: DLPF verify CONFIG: orig=0x%02X new=0x%02X readback=0x%02X, "
      "SMPLRT_DIV: target=0x%02X readback=0x%02X, GYRO_CONFIG=0x%02X\n",
      originalConfig, newConfig, verifyConfig,
      smplrt_div, verifyDiv, gyroCfg);

  if (!okCfg || !okDiv) {
    setDlpfError("VERIFY_READ_FAILED");
    Serial.println("IMU DLPF: verification readback failed");
    return false;
  }

  bool match = (verifyConfig == newConfig) && (verifyDiv == smplrt_div);
  if (!match) {
    setDlpfError("VERIFY_MISMATCH");
    Serial.println("IMU DLPF: verification mismatch");
    return false;
  }
  // Successful configuration: set settle counter and clear error
  g_dlpfSettleRemaining = DLPF_SETTLE_SAMPLES;
  setDlpfError(nullptr);
  Serial.printf("IMU: DLPF configured. DLPF_CFG=%u SMPLRT_DIV=%u DELAY_MS=%s (approx). "
                "Ignoring next %u samples for filter settling.\n",
                dlpf_cfg, smplrt_div, dlpfDelayMsString(dlpf_cfg),
                DLPF_SETTLE_SAMPLES);
  return match;
}

bool getDLPFConfig(uint8_t &dlpf_cfg, uint8_t &smplrt_div) {
  setDlpfError(nullptr);

  uint8_t cfg = 0;
  uint8_t div = 0;
  if (!readReg(REG_CONFIG, cfg)) {
    setDlpfError("I2C_READ_CONFIG_FAILED");
    return false;
  }
  if (!readReg(REG_SMPLRT_DIV, div)) {
    setDlpfError("I2C_READ_SMPLRT_FAILED");
    return false;
  }
  dlpf_cfg = cfg & CONFIG_DLPF_MASK;
  smplrt_div = div;
  return true;
}

// Persist DLPF configuration to NVS so it can be restored on next boot.
bool saveImuDlpfConfigToNvs(uint8_t dlpf_cfg, uint8_t smplrt_div) {
  Preferences prefs;
  if (!prefs.begin(PREFS_NAMESPACE, false)) {
    setDlpfError("NVS_OPEN_FAILED");
    Serial.println("IMU DLPF: failed to open NVS for save");
    return false;
  }
  prefs.putUInt(PREFS_KEY_IMU_DLPF_MAGIC, IMU_DLPF_MAGIC);
  prefs.putUChar(PREFS_KEY_IMU_DLPF_CFG, dlpf_cfg & CONFIG_DLPF_MASK);
  prefs.putUChar(PREFS_KEY_IMU_SMPLRT, smplrt_div);
  prefs.end();
  return true;
}

// Load DLPF configuration from NVS if it exists. Returns true if a valid
// configuration was loaded, false otherwise.
bool loadImuDlpfConfigFromNvs(uint8_t &dlpf_cfg, uint8_t &smplrt_div) {
  Preferences prefs;
  if (!prefs.begin(PREFS_NAMESPACE, true)) { // read-only
    return false;
  }
  uint32_t magic = prefs.getUInt(PREFS_KEY_IMU_DLPF_MAGIC, 0);
  if (magic != IMU_DLPF_MAGIC) {
    prefs.end();
    return false;
  }
  uint8_t cfg = prefs.getUChar(PREFS_KEY_IMU_DLPF_CFG, DEFAULT_DLPF_CFG);
  uint8_t div = prefs.getUChar(PREFS_KEY_IMU_SMPLRT, DEFAULT_SMPLRT_DIV);
  prefs.end();
  dlpf_cfg = cfg & CONFIG_DLPF_MASK;
  smplrt_div = div;
  return true;
}

IMU::IMU() {
  pitch = roll = yaw = 0.0f;
  lastMillis = 0;
  pitchOffset = 0.0f;
  rollOffset = 0.0f;
}

bool IMU::begin() {
  // explicit Wire init (ESP32 pin choices deterministic)
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_CLOCK_HZ);
  if (mpu) { delete mpu; mpu = nullptr; }

  // try default addr 0x68 then 0x69
  g_mpuAddr = MPU_ADDR_DEFAULT;
  mpu = new MPU6050(Wire, g_mpuAddr);
  if (!mpu->begin()) {
    delete mpu;
    g_mpuAddr = MPU_ADDR_ALT;
    mpu = new MPU6050(Wire, g_mpuAddr);
    if (!mpu->begin()) {
      Serial.println("MPU6050 not found (custom lib)");
      delete mpu; mpu = nullptr;
      return false;
    }
  }
  delay(50);
  lastMillis = millis();
  Serial.println("MPU6050 initialized (custom lib)");
  // On boot, only configure DLPF/sample rate if a user-chosen configuration
  // exists in NVS. This lets the robot power up without requiring a serial
  // connection every time: once tuned and saved, the config is restored here.
  uint8_t dlpf_cfg = 0, dlpf_div = 0;
  if (loadImuDlpfConfigFromNvs(dlpf_cfg, dlpf_div)) {
    if (!configureDLPF(dlpf_cfg, dlpf_div, false)) {
      Serial.println("IMU: WARNING - failed to restore DLPF config from NVS");
    } else {
      Serial.printf("IMU: Restored DLPF from NVS (cfg=%u div=%u)\n",
                    dlpf_cfg, dlpf_div);
    }
  } else {
    Serial.println("IMU: No DLPF config in NVS; using library defaults. Use IMU:SET DLPF to configure.");
  }

  // Try to load calibration from NVS
  // If no calibration exists, offsets remain at 0.0 (user must calibrate explicitly)
  if (loadCalibration()) {
    Serial.println("IMU calibration loaded from NVS");
  } else {
    Serial.println("IMU: No calibration found. Use CALIBRATE command to calibrate.");
    pitchOffset = 0.0f;
    rollOffset = 0.0f;
  }

  return true;
}

void IMU::update(float dt) {
  if (!mpu) return;

  float oldPitch = pitch;
  float oldRoll  = roll;

  // delegate to library
  mpu->update(dt);

  float newPitch = mpu->getPitch();
  float newRoll  = mpu->getRoll();
  float newYaw   = mpu->getYaw();

  // apply calibration offsets
  newPitch -= pitchOffset;
  newRoll  -= rollOffset;

  // detect stall / frozen readings: tiny change over a period
  if (fabsf(newPitch - oldPitch) < IMU_STALL_ANGLE_CHANGE_MIN && 
      fabsf(newRoll - oldRoll) < IMU_STALL_ANGLE_CHANGE_MIN &&
      (lastMillis != 0) && (millis() - lastMillis) > IMU_STALL_TIMEOUT_MS) {
    Serial.println("IMU stalled — attempting I2C recover + reinit");
    IMU::i2cBusRecover(I2C_SDA_PIN, I2C_SCL_PIN);
    // try reinit addresses again
    if (mpu) { delete mpu; mpu = nullptr; }
    mpu = new MPU6050(Wire, MPU_ADDR_DEFAULT);
    if (!mpu->begin()) {
      delete mpu;
      mpu = new MPU6050(Wire, MPU_ADDR_ALT);
      if (!mpu->begin()) {
        Serial.println("IMU reinit failed.");
        delete mpu; mpu = nullptr;
        return;
      }
    }
    Serial.println("IMU reinit successful.");
    lastMillis = millis();
    return;
  }

  // accept new angles
  // After a DLPF configuration change, allow a couple of samples to pass
  // through the internal fusion but do not update the public angles yet.
  if (g_dlpfSettleRemaining > 0) {
    g_dlpfSettleRemaining--;
    lastMillis = millis();
    return;
  }

  pitch = newPitch;
  roll  = newRoll;
  yaw   = newYaw;
  lastMillis = millis();
}

float IMU::getPitch(){ return pitch; }
float IMU::getRoll(){ return roll; }
float IMU::getYaw(){ return yaw; }
unsigned long IMU::lastUpdateMillis(){ return lastMillis; }

// i2c bus recovery: bit-bang SCL to free stuck SDA, then toggle lines and re-init Wire
void IMU::i2cBusRecover(int sdaPin, int sclPin) {
  Wire.end();
  pinMode(sclPin, OUTPUT);
  pinMode(sdaPin, INPUT_PULLUP);
  for (int i = 0; i < 9; ++i) {
    digitalWrite(sclPin, HIGH);
    delayMicroseconds(5);
    digitalWrite(sclPin, LOW);
    delayMicroseconds(5);
    if (digitalRead(sdaPin) == HIGH) break;
  }
  pinMode(sdaPin, OUTPUT);
  digitalWrite(sdaPin, LOW);
  delayMicroseconds(5);
  digitalWrite(sclPin, HIGH);
  delayMicroseconds(5);
  digitalWrite(sdaPin, HIGH);
  delayMicroseconds(5);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_CLOCK_HZ);
}

// Blocking calibration: requires robot to be kept still during execution
// Averages IMU_CALIB_SAMPLES readings to compute zero offsets
bool IMU::calibrateBlocking() {
  if (!mpu) {
    Serial.println("ERR CALIBRATE: IMU not initialized");
    return false;
  }

  Serial.println("CALIBRATE: Starting calibration. Keep robot still...");
  Serial.printf("CALIBRATE: Collecting %d samples (will take ~%d seconds)...\n", 
                IMU_CALIB_SAMPLES, (IMU_CALIB_SAMPLES * IMU_CALIB_DELAY_MS) / 1000);

  float sumPitch = 0.0f;
  float sumRoll = 0.0f;

  for (int i = 0; i < IMU_CALIB_SAMPLES; ++i) {
    // Allow the lib to update internal filters; pass dt=0.0 for internal handling
    mpu->update(0.0f);
    sumPitch += mpu->getPitch();
    sumRoll += mpu->getRoll();
    delay(IMU_CALIB_DELAY_MS);
    
    // Progress indicator every 50 samples
    if ((i + 1) % 50 == 0) {
      Serial.printf("CALIBRATE: Progress %d/%d\n", i + 1, IMU_CALIB_SAMPLES);
    }
  }

  // Compute offsets
  pitchOffset = sumPitch / (float)IMU_CALIB_SAMPLES;
  rollOffset = sumRoll / (float)IMU_CALIB_SAMPLES;

  Serial.printf("CALIBRATE: Done. pitchOffset=%.3f rollOffset=%.3f\n", pitchOffset, rollOffset);
  Serial.println("CALIBRATE: Use SAVE_CAL to persist calibration to NVS");

  return true;
}

// Save calibration to NVS
bool IMU::saveCalibration() {
  Preferences prefs;
  if (!prefs.begin(PREFS_NAMESPACE, false)) {
    Serial.println("ERR SAVE_CAL: Failed to open NVS");
    return false;
  }

  prefs.putFloat(PREFS_KEY_CALIB_PITCH_OFFSET, pitchOffset);
  prefs.putFloat(PREFS_KEY_CALIB_ROLL_OFFSET, rollOffset);
  prefs.putUInt(PREFS_KEY_CALIB_MAGIC, IMU_CALIB_MAGIC);
  prefs.putUChar(PREFS_KEY_CALIB_VERSION, IMU_CALIB_VERSION);
  prefs.end();

  Serial.printf("OK SAVE_CAL: Saved pitchOffset=%.3f rollOffset=%.3f\n", pitchOffset, rollOffset);
  return true;
}

// Load calibration from NVS
bool IMU::loadCalibration() {
  Preferences prefs;
  if (!prefs.begin(PREFS_NAMESPACE, true)) { // read-only
    return false;
  }

  // Check magic number to verify valid calibration
  uint32_t magic = prefs.getUInt(PREFS_KEY_CALIB_MAGIC, 0);
  if (magic != IMU_CALIB_MAGIC) {
    prefs.end();
    return false; // No valid calibration
  }

  // Check version for future compatibility
  uint8_t version = prefs.getUChar(PREFS_KEY_CALIB_VERSION, 0);
  if (version != IMU_CALIB_VERSION) {
    Serial.printf("WARN: Calibration version mismatch (stored: %d, expected: %d)\n", 
                  version, IMU_CALIB_VERSION);
    // Could implement version migration here if needed
  }

  // Load offsets
  pitchOffset = prefs.getFloat(PREFS_KEY_CALIB_PITCH_OFFSET, 0.0f);
  rollOffset = prefs.getFloat(PREFS_KEY_CALIB_ROLL_OFFSET, 0.0f);
  prefs.end();

  return true;
}

// Clear calibration from NVS
bool IMU::clearCalibration() {
  Preferences prefs;
  if (!prefs.begin(PREFS_NAMESPACE, false)) {
    Serial.println("ERR CLEAR_CAL: Failed to open NVS");
    return false;
  }

  prefs.remove(PREFS_KEY_CALIB_PITCH_OFFSET);
  prefs.remove(PREFS_KEY_CALIB_ROLL_OFFSET);
  prefs.remove(PREFS_KEY_CALIB_MAGIC);
  prefs.remove(PREFS_KEY_CALIB_VERSION);
  prefs.end();

  // Reset offsets to zero
  pitchOffset = 0.0f;
  rollOffset = 0.0f;

  Serial.println("OK CLEAR_CAL: Calibration cleared");
  return true;
}

// Check if valid calibration exists
bool IMU::hasCalibration() {
  Preferences prefs;
  if (!prefs.begin(PREFS_NAMESPACE, true)) { // read-only
    return false;
  }

  uint32_t magic = prefs.getUInt(PREFS_KEY_CALIB_MAGIC, 0);
  prefs.end();

  return (magic == IMU_CALIB_MAGIC);
}

// Get current calibration info
void IMU::getCalibrationInfo(CalibrationData &out) {
  out.pitchOffset = pitchOffset;
  out.rollOffset = rollOffset;
  out.magic = IMU_CALIB_MAGIC;
  out.version = IMU_CALIB_VERSION;
}

#include "SerialBridge.h"
#include "BotController.h"
#include "IMU.h"
#include <Arduino.h>

extern bool configureDLPF(uint8_t dlpf_cfg, uint8_t smplrt_div, bool force);
extern bool getDLPFConfig(uint8_t& dlpf_cfg, uint8_t& smplrt_div);
extern const char* getImuDlpfLastError();
extern bool saveImuDlpfConfigToNvs(uint8_t dlpf_cfg, uint8_t smplrt_div);

static uint8_t defaultSmplrtForDlpf(uint8_t dlpf_cfg) {
    return ((dlpf_cfg & 0x07u) == 0) ? 39 : 4;
}

static const char* dlpfDelayMsString(uint8_t cfg) {
    switch (cfg & 0x07) {
        case 0: return "0.98";
        case 1: return "1.90";
        case 2: return "2.80";
        case 3: return "4.90";
        case 4: return "8.30";
        case 5: return "13.40";
        case 6: return "18.60";
        default: return "approx";
    }
}

static void sendErr(const char* msg) {
    Serial.print("ERR ");
    Serial.println(msg);
}

SerialBridge::SerialBridge() : _buffer(""), _getPidRequested(false), _paramsOut(nullptr), _controller(nullptr), _imu(nullptr) {}

void SerialBridge::begin(unsigned long baud) {
    Serial.begin(baud);
}

bool SerialBridge::consumeGetPidRequest() {
    noInterrupts();
    bool v = _getPidRequested;
    _getPidRequested = false;
    interrupts();
    return v;
}

void SerialBridge::printCurrent(PIDController& pid) {
    float kp, ki, kd;
    pid.getTunings(kp, ki, kd);
    Serial.printf("KP: %.6f KI: %.6f KD: %.6f\n", kp, ki, kd);
}

void SerialBridge::sendError(const char* msg) {
    sendErr(msg);
}

bool SerialBridge::poll(PIDParams& paramsOut, IMU* imu, BotController* controller) {
    _paramsOut = &paramsOut;
    _imu = imu;
    _controller = controller;
    
    bool gotSet = false;
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\r') continue;
        if (c == '\n') {
            String line = _buffer;
            line.trim();
            _buffer = "";
            if (line.length() == 0) continue;
            
            char lineCopy[257];
            line.toCharArray(lineCopy, sizeof(lineCopy));
            dispatch(lineCopy);
            
            if (paramsOut.kp != 0 || paramsOut.ki != 0 || paramsOut.kd != 0) {
                gotSet = true;
            }
        } else {
            _buffer += c;
            if (_buffer.length() > 256) _buffer = _buffer.substring(_buffer.length() - 256);
        }
    }
    return gotSet;
}

void SerialBridge::dispatch(const char* line) {
    char cmd[32] = {0};
    char args[256] = {0};
    
    const char* space = strchr(line, ' ');
    if (space) {
        size_t cmdLen = space - line;
        if (cmdLen >= sizeof(cmd)) cmdLen = sizeof(cmd) - 1;
        strncpy(cmd, line, cmdLen);
        cmd[cmdLen] = '\0';
        strncpy(args, space + 1, sizeof(args) - 1);
    } else {
        strncpy(cmd, line, sizeof(cmd) - 1);
    }
    
    for (size_t i = 0; i < strlen(cmd); i++) {
        cmd[i] = toupper((unsigned char)cmd[i]);
    }
    
    for (uint8_t i = 0; i < _commandCount; i++) {
        CommandEntry entry;
        memcpy_P(&entry, &_commands[i], sizeof(CommandEntry));
        if (strcmp(cmd, entry.name) == 0) {
            entry.handler(args, this);
            return;
        }
    }
    
    if (strcmp(cmd, "HELP") == 0) {
        Serial.println("Commands:");
        for (uint8_t i = 0; i < _commandCount; i++) {
            CommandEntry entry;
            memcpy_P(&entry, &_commands[i], sizeof(CommandEntry));
            Serial.print("  ");
            Serial.println(entry.help);
        }
        return;
    }
    
    sendErr("UNKNOWN CMD");
}

#define CMD(n, h) { n, &SerialBridge::h, h },

const CommandEntry SerialBridge::_commands[] PROGMEM = {
    CMD("SET PID", cmdSetPid)
    CMD("GET PID", cmdGetPid)
    CMD("CALIBRATE", cmdCalibrate)
    CMD("SAVE_CAL", cmdSaveCal)
    CMD("LOAD_CAL", cmdLoadCal)
    CMD("CLEAR_CAL", cmdClearCal)
    CMD("GET_CAL_INFO", cmdGetCalInfo)
    CMD("SET TANK", cmdSetTank)
    CMD("SET MODE", cmdSetMode)
    CMD("GET MODE", cmdGetMode)
    CMD("ESTOP", cmdEstop)
    CMD("ESTOP CLEAR", cmdEstopClear)
    CMD("RUN_SELF_CHECKS", cmdRunSelfChecks)
    CMD("GET_BOOT_TAG", cmdGetBootTag)
    CMD("GET STATUS", cmdGetStatus)
    CMD("TEST_MODE_ON", cmdTestModeOn)
    CMD("TEST_MODE_OFF", cmdTestModeOff)
    CMD("IMU:GET DLPF", cmdImuGetDlpF)
    CMD("IMU:SET DLPF", cmdImuSetDlpF)
    CMD("IMU:HELP", cmdImuHelp)
};
#undef CMD

void SerialBridge::cmdSetPid(const char* args, void* userdata) {
    SerialBridge* s = (SerialBridge*)userdata;
    float kp, ki, kd;
    if (sscanf(args, "%f %f %f", &kp, &ki, &kd) == 3) {
        if (s->_paramsOut) {
            s->_paramsOut->kp = kp;
            s->_paramsOut->ki = ki;
            s->_paramsOut->kd = kd;
        }
        Serial.printf("ACK SET PID %.6f %.6f %.6f\n", kp, ki, kd);
    } else {
        sendErr("SET PID requires three floats");
    }
}

void SerialBridge::cmdGetPid(const char* args, void* userdata) {
    (void)args;
    SerialBridge* s = (SerialBridge*)userdata;
    s->_getPidRequested = true;
}

void SerialBridge::cmdCalibrate(const char* args, void* userdata) {
    (void)args;
    SerialBridge* s = (SerialBridge*)userdata;
    if (s->_imu && s->_imu->calibrateBlocking()) {
        Serial.println("OK CALIBRATE");
    } else {
        sendErr("CALIBRATE failed");
    }
}

void SerialBridge::cmdSaveCal(const char* args, void* userdata) {
    (void)args;
    SerialBridge* s = (SerialBridge*)userdata;
    if (s->_imu && s->_imu->saveCalibration()) {
        Serial.println("OK SAVE_CAL");
    } else {
        sendErr("SAVE_CAL failed");
    }
}

void SerialBridge::cmdLoadCal(const char* args, void* userdata) {
    (void)args;
    SerialBridge* s = (SerialBridge*)userdata;
    if (s->_imu && s->_imu->loadCalibration()) {
        Serial.println("OK LOAD_CAL");
    } else {
        sendErr("No valid calibration");
    }
}

void SerialBridge::cmdClearCal(const char* args, void* userdata) {
    (void)args;
    SerialBridge* s = (SerialBridge*)userdata;
    if (s->_imu && s->_imu->clearCalibration()) {
        Serial.println("OK CLEAR_CAL");
    } else {
        sendErr("CLEAR_CAL failed");
    }
}

void SerialBridge::cmdGetCalInfo(const char* args, void* userdata) {
    (void)args;
    SerialBridge* s = (SerialBridge*)userdata;
    if (s->_imu) {
        CalibrationData cal;
        s->_imu->getCalibrationInfo(cal);
        Serial.printf("DATA CAL_INFO pitchOffset=%.6f rollOffset=%.6f magic=0x%08X\n", 
                      cal.pitchOffset, cal.rollOffset, cal.magic);
    }
}

void SerialBridge::cmdSetTank(const char* args, void* userdata) {
    SerialBridge* s = (SerialBridge*)userdata;
    float left, right;
    if (sscanf(args, "%f %f", &left, &right) == 2) {
        if (s->_controller) {
            TankControl tank = {left, right};
            s->_controller->applyTankControl(tank);
        }
        Serial.printf("ACK SET TANK %.1f %.1f\n", left, right);
    } else {
        sendErr("SET TANK requires two floats");
    }
}

void SerialBridge::cmdSetMode(const char* args, void* userdata) {
    SerialBridge* s = (SerialBridge*)userdata;
    char modeStr[16] = {0};
    sscanf(args, "%15s", modeStr);
    
    for (char* p = modeStr; *p; p++) *p = toupper((unsigned char)*p);
    
    ControlMode mode;
    if (strcmp(modeStr, "AUTO") == 0) {
        mode = ControlMode::AUTO;
    } else if (strcmp(modeStr, "MANUAL") == 0) {
        mode = ControlMode::MANUAL;
    } else if (strcmp(modeStr, "MIXED") == 0) {
        mode = ControlMode::MIXED;
    } else {
        sendErr("MODE must be AUTO, MANUAL, or MIXED");
        return;
    }
    
    if (s->_controller) {
        s->_controller->setControlMode(mode);
        Serial.printf("OK MODE %s\n", modeStr);
    }
}

void SerialBridge::cmdGetMode(const char* args, void* userdata) {
    (void)args;
    SerialBridge* s = (SerialBridge*)userdata;
    if (s->_controller) {
        const char* names[] = {"AUTO", "MANUAL", "MIXED"};
        uint8_t idx = static_cast<uint8_t>(s->_controller->getControlMode());
        Serial.printf("DATA MODE %s\n", names[idx]);
    }
}

void SerialBridge::cmdEstop(const char* args, void* userdata) {
    (void)args;
    SerialBridge* s = (SerialBridge*)userdata;
    if (s->_controller) {
        s->_controller->emergencyStop();
        Serial.println("OK ESTOP");
    }
}

void SerialBridge::cmdEstopClear(const char* args, void* userdata) {
    (void)args;
    SerialBridge* s = (SerialBridge*)userdata;
    if (s->_controller) {
        s->_controller->clearEmergencyStop();
        Serial.println("OK ESTOP CLEAR");
    }
}

void SerialBridge::cmdRunSelfChecks(const char* args, void* userdata) {
    (void)args;
    SerialBridge* s = (SerialBridge*)userdata;
    if (s->_controller) {
        s->_controller->runSelfChecks();
    }
}

void SerialBridge::cmdGetBootTag(const char* args, void* userdata) {
    (void)args;
    Serial.printf("DATA BOOT_TAG: %s\n", BOOT_TAG);
}

void SerialBridge::cmdGetStatus(const char* args, void* userdata) {
    (void)args;
    SerialBridge* s = (SerialBridge*)userdata;
    if (s->_controller && s->_imu) {
        Serial.println("DATA STATUS:");
        Serial.printf("  test_mode: %s\n", s->_controller->isTestMode() ? "true" : "false");
        Serial.printf("  boot_tag: %s\n", BOOT_TAG);
        Serial.printf("  imu_pitch: %.2f\n", s->_imu->getPitch());
        Serial.printf("  imu_roll: %.2f\n", s->_imu->getRoll());
        Serial.printf("  imu_yaw: %.2f\n", s->_imu->getYaw());
        Serial.printf("  has_calibration: %s\n", s->_imu->hasCalibration() ? "true" : "false");
        const char* mn[] = {"AUTO", "MANUAL", "MIXED"};
        Serial.printf("  control_mode: %s\n", mn[static_cast<uint8_t>(s->_controller->getControlMode())]);
        Serial.printf("  emergency_stop: %s\n", s->_controller->isEmergencyStopActive() ? "ACTIVE" : "CLEAR");
    }
}

void SerialBridge::cmdTestModeOn(const char* args, void* userdata) {
    (void)args;
    SerialBridge* s = (SerialBridge*)userdata;
    if (s->_controller) {
        s->_controller->setTestMode(true);
        Serial.println("OK TEST_MODE ON");
    }
}

void SerialBridge::cmdTestModeOff(const char* args, void* userdata) {
    (void)args;
    SerialBridge* s = (SerialBridge*)userdata;
    if (s->_controller) {
        s->_controller->setTestMode(false);
        Serial.println("OK TEST_MODE OFF");
    }
}

void SerialBridge::cmdImuGetDlpF(const char* args, void* userdata) {
    (void)args;
    uint8_t cfg = 0, div = 0;
    if (getDLPFConfig(cfg, div)) {
        Serial.printf("OK DLPF=%u SMPLRT=%u DELAY_MS=%s\n", cfg, div, dlpfDelayMsString(cfg));
    } else {
        sendErr(getImuDlpfLastError() ? getImuDlpfLastError() : "GET_FAILED");
    }
}

void SerialBridge::cmdImuSetDlpF(const char* args, void* userdata) {
    (void)userdata;
    int cfg = -1, div = -1;
    char extra[16] = {0};
    
    if (sscanf(args, "%d %d %15s", &cfg, &div, extra) < 1 || cfg < 0 || cfg > 7) {
        sendErr("BAD_PARAMS");
        return;
    }
    
    bool force = false;
    if (extra[0]) {
        for (char* p = extra; *p; p++) *p = toupper((unsigned char)*p);
        if (strcmp(extra, "FORCE") == 0) {
            force = true;
        }
    }
    
    if (div < 0) div = defaultSmplrtForDlpf((uint8_t)cfg);
    if (div < 0 || div > 255) {
        sendErr("BAD_PARAMS");
        return;
    }
    
    if (!configureDLPF((uint8_t)cfg, (uint8_t)div, force)) {
        sendErr(getImuDlpfLastError() ? getImuDlpfLastError() : "CONFIG_FAILED");
        return;
    }
    
    saveImuDlpfConfigToNvs((uint8_t)cfg, (uint8_t)div);
    Serial.printf("OK DLPF=%d SMPLRT=%d DELAY_MS=%s\n", cfg, div, dlpfDelayMsString((uint8_t)cfg));
}

void SerialBridge::cmdImuHelp(const char* args, void* userdata) {
    (void)args;
    (void)userdata;
    Serial.println("IMU Commands:");
    Serial.println("  IMU:GET DLPF");
    Serial.println("  IMU:SET DLPF <cfg> [div] [FORCE]");
    Serial.println("  IMU:HELP");
}

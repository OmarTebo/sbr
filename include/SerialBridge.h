#pragma once
#include <Arduino.h>
#include "PIDController.h"
#include "Types.h"
#include "Config.h"

class IMU;
class BotController;

struct CommandEntry {
    const char* name;
    void (*handler)(const char* args, void* userdata);
    const char* help;
};

class SerialBridge {
public:
    SerialBridge();
    void begin(unsigned long baud);
    bool poll(PIDParams& paramsOut, IMU* imu = nullptr, BotController* controller = nullptr);
    void printCurrent(PIDController& pid);
    bool consumeGetPidRequest();
    
    void setController(BotController* ctrl) { _controller = ctrl; }
    void setIMU(IMU* imu) { _imu = imu; }

private:
    void dispatch(const char* line);
    void sendError(const char* msg);

    String _buffer;
    volatile bool _getPidRequested = false;
    PIDParams* _paramsOut = nullptr;
    BotController* _controller = nullptr;
    IMU* _imu = nullptr;
    
    static const CommandEntry _commands[] PROGMEM;
    static const uint8_t _commandCount = 21;
    
    static void cmdSetPid(const char* args, void* userdata);
    static void cmdGetPid(const char* args, void* userdata);
    static void cmdCalibrate(const char* args, void* userdata);
    static void cmdSaveCal(const char* args, void* userdata);
    static void cmdLoadCal(const char* args, void* userdata);
    static void cmdClearCal(const char* args, void* userdata);
    static void cmdGetCalInfo(const char* args, void* userdata);
    static void cmdSetTank(const char* args, void* userdata);
    static void cmdSetMode(const char* args, void* userdata);
    static void cmdGetMode(const char* args, void* userdata);
    static void cmdEstop(const char* args, void* userdata);
    static void cmdEstopClear(const char* args, void* userdata);
    static void cmdRunSelfChecks(const char* args, void* userdata);
    static void cmdGetBootTag(const char* args, void* userdata);
    static void cmdGetStatus(const char* args, void* userdata);
    static void cmdTestModeOn(const char* args, void* userdata);
    static void cmdTestModeOff(const char* args, void* userdata);
    static void cmdImuGetDlpF(const char* args, void* userdata);
    static void cmdImuSetDlpF(const char* args, void* userdata);
    static void cmdImuHelp(const char* args, void* userdata);
};

# SBR Controller - Flutter BLE App

BLE controller app for the Self-Balancing Robot.

## Setup

### Prerequisites
- Flutter SDK 3.0+
- Android Studio (for Android)
- Xcode (for iOS)
- Physical device with BLE support

### Installation

```bash
cd flutter_app
flutter pub get
```

### Run

```bash
flutter run
```

### Build APK

```bash
flutter build apk --debug
```

## Features

- **BLE Connection**: Scan and connect to SBR-Bot
- **Tank Control**: Dual motor speed sliders (-100% to +100%)
- **Control Modes**: AUTO (PID), MANUAL (tank), MIXED (tank + PID)
- **PID Tuning**: Real-time Kp/Ki/Kd adjustment
- **Telemetry**: Real-time pitch/roll/yaw display with graphs
- **Emergency Stop**: One-tap motor disable
- **Calibration**: Trigger IMU calibration from app

## BLE Characteristics

| UUID | Name | Direction | Format |
|------|------|----------|--------|
| d1c6f3e1... | KP | Write | Float string |
| d1c6f3e2... | KI | Write | Float string |
| d1c6f3e3... | KD | Write | Float string |
| d1c6f3e4... | Tank Left | Write | Float string (-100 to 100) |
| d1c6f3e5... | Tank Right | Write | Float string (-100 to 100) |
| d1c6f3e6... | Control Mode | Write | Byte (0=AUTO, 1=MANUAL, 2=MIXED) |
| d1c6f3e7... | Emergency Stop | Write | Byte (1=stop) |
| d1c6f3e8... | Calibrate | Write | Byte (1=calibrate) |
| d1c6f3e9... | Telemetry | Notify | CSV string |

## Control Modes

- **AUTO**: Pure PID balance control
- **MANUAL**: Tank control only (no balancing)
- **MIXED**: Tank + PID corrections

# SBR Controller - Python BLE App

Python BLE controller for the Self-Balancing Robot.

## Requirements

- Python 3.8+
- Windows 10/11 with Bluetooth
- bleak (`pip install bleak`)

## Installation

```bash
pip install bleak
```

## Running

```bash
python -m python_app
```

Or from the main-code-repo directory:

```bash
python test_python_app.py  # Quick test
python -m python_app       # Run GUI
```

## Features

- **BLE Connection**: Scan and connect to SBR-Bot
- **Tank Control**: Dual motor speed sliders (-100% to +100%)
- **Control Modes**: AUTO (PID), MANUAL (tank), MIXED (tank + PID)
- **PID Tuning**: Real-time Kp/Ki/Kd adjustment
- **Telemetry**: Real-time pitch/roll/yaw display
- **Emergency Stop**: One-tap motor disable
- **Calibration**: Trigger IMU calibration

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

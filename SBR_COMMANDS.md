# SBR Serial Commands Quick Reference

## Connection
```bash
pip install pyserial
python tools/pid_gui.py
```
Select COM port → Connect → Done.

---

## Commands

### PID Control
| Command | Description |
|---------|-------------|
| `GET PID` | Get current PID values |
| `SET PID <kp> <ki> <kd>` | Set PID gains |

### Calibration
| Command | Description |
|---------|-------------|
| `CALIBRATE` | Calibrate IMU (robot must be level) |
| `SAVE_CAL` | Save calibration to NVS |
| `LOAD_CAL` | Load calibration from NVS |
| `CLEAR_CAL` | Clear saved calibration |
| `GET_CAL_INFO` | Show calibration offsets |

### Control Modes
| Command | Description |
|---------|-------------|
| `SET MODE AUTO` | Pure PID balance control |
| `SET MODE MANUAL` | Tank control (no PID) |
| `SET MODE MIXED` | Tank + PID corrections |
| `GET MODE` | Show current mode |

### Tank Control
| Command | Description |
|---------|-------------|
| `SET TANK <left> <right>` | Motor speed (-100 to 100) |

### Safety
| Command | Description |
|---------|-------------|
| `ESTOP` | Emergency stop (kills motors) |
| `ESTOP CLEAR` | Clear emergency stop |
| `TEST_MODE_ON` | Enable test mode (motors off) |
| `TEST_MODE_OFF` | Disable test mode |

### IMU / DLPF
| Command | Description |
|---------|-------------|
| `IMU:GET DLPF` | Get current DLPF config |
| `IMU:SET DLPF <cfg> [div]` | Set DLPF (0-7, optional divider) |
| `IMU:HELP` | Show IMU commands |

### Info
| Command | Description |
|---------|-------------|
| `GET STATUS` | Full system status |
| `GET BOOT_TAG` | Firmware version |
| `RUN_SELF_CHECKS` | Run self-test |
| `HELP` | Show all commands |

---

## Response Formats

### Telemetry (20Hz)
```
PITCH:0.00 ROLL:0.00 YAW:0.00
```

### GET STATUS
```
DATA STATUS:
  test_mode: false
  boot_tag: SBR-v1.0
  imu_pitch: 0.00
  imu_roll: 0.00
  imu_yaw: 0.00
  has_calibration: true
  control_mode: AUTO
  emergency_stop: CLEAR
```

### PID Response
```
ACK SET PID 1.000000 0.000000 1.000000
```

### Error Response
```
ERR <error message>
```

---

## Control Modes

| Mode | Description | Use Case |
|------|-------------|----------|
| `AUTO` | PID balance only | Standalone balancing |
| `MANUAL` | Tank control only | Manual driving |
| `MIXED` | Tank + PID | Drive with balance assist |

---

## DLPF Settings

| CFG | Bandwidth | Delay |
|-----|-----------|-------|
| 0 | 256 Hz | 0.98ms |
| 1 | 188 Hz | 1.90ms |
| 2 | 98 Hz | 2.80ms |
| 3 | 42 Hz | 4.90ms |
| 4 | 20 Hz | 8.30ms |
| 5 | 10 Hz | 13.40ms |
| 6 | 5 Hz | 18.60ms |
| 7 | ~2600 Hz | ~ |

Default: CFG=6 (good for balancing)

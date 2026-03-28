enum ControlMode {
  autoMode(0, 'AUTO'),
  manualMode(1, 'MANUAL'),
  mixedMode(2, 'MIXED');

  const ControlMode(this.value, this.name);
  final int value;
  final String name;

  static ControlMode fromValue(int value) {
    return ControlMode.values.firstWhere(
      (e) => e.value == value,
      orElse: () => ControlMode.autoMode,
    );
  }
}

class PIDParams {
  double kp;
  double ki;
  double kd;

  PIDParams({this.kp = 1.0, this.ki = 0.0, this.kd = 1.0});

  @override
  String toString() => 'KP: $kp, KI: $ki, KD: $kd';
}

class TankControl {
  double leftMotor;
  double rightMotor;

  TankControl({this.leftMotor = 0.0, this.rightMotor = 0.0});
}

class TelemetryData {
  double pitch;
  double roll;
  double yaw;
  double leftSpeed;
  double rightSpeed;
  ControlMode mode;
  bool emergencyStop;

  TelemetryData({
    this.pitch = 0.0,
    this.roll = 0.0,
    this.yaw = 0.0,
    this.leftSpeed = 0.0,
    this.rightSpeed = 0.0,
    this.mode = ControlMode.autoMode,
    this.emergencyStop = false,
  });

  factory TelemetryData.fromString(String data) {
    final parts = data.split(',');
    if (parts.length < 7) {
      return TelemetryData();
    }

    return TelemetryData(
      pitch: double.tryParse(parts[0]) ?? 0.0,
      roll: double.tryParse(parts[1]) ?? 0.0,
      yaw: double.tryParse(parts[2]) ?? 0.0,
      leftSpeed: double.tryParse(parts[3]) ?? 0.0,
      rightSpeed: double.tryParse(parts[4]) ?? 0.0,
      mode: ControlMode.fromValue(int.tryParse(parts[5]) ?? 0),
      emergencyStop: (int.tryParse(parts[6]) ?? 0) == 1,
    );
  }
}

import 'dart:async';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import '../models/models.dart';
import '../utils/constants.dart';

class BLEService {
  BluetoothDevice? _device;
  BluetoothCharacteristic? _kpChar;
  BluetoothCharacteristic? _kiChar;
  BluetoothCharacteristic? _kdChar;
  BluetoothCharacteristic? _tankLeftChar;
  BluetoothCharacteristic? _tankRightChar;
  BluetoothCharacteristic? _controlModeChar;
  BluetoothCharacteristic? _emergencyStopChar;
  BluetoothCharacteristic? _calibrateChar;
  BluetoothCharacteristic? _telemetryChar;

  final StreamController<TelemetryData> _telemetryController =
      StreamController<TelemetryData>.broadcast();
  final StreamController<bool> _connectionController =
      StreamController<bool>.broadcast();

  Stream<TelemetryData> get telemetryStream => _telemetryController.stream;
  Stream<bool> get connectionStream => _connectionController.stream;

  bool get isConnected => _device != null;

  Future<void> scanAndConnect() async {
    try {
      await FlutterBluePlus.startScan(
        withServices: [Guid(SBRConstants.serviceUuid)],
        timeout: SBRConstants.scanTimeout,
      );

      final results = await FlutterBluePlus.scanResults.first;
      final device = results.device;

      if (device != null) {
        await connect(device);
      } else {
        throw Exception('SBR-Bot not found');
      }
    } catch (e) {
      rethrow;
    }
  }

  Future<void> connect(BluetoothDevice device) async {
    try {
      await device.connect(timeout: SBRConstants.connectionTimeout);
      _device = device;

      final services = await device.discoverServices();
      final service = services.firstWhere(
        (s) => s.uuid.toString() == SBRConstants.serviceUuid,
      );

      _kpChar = _findCharacteristic(service, SBRConstants.charKpUuid);
      _kiChar = _findCharacteristic(service, SBRConstants.charKiUuid);
      _kdChar = _findCharacteristic(service, SBRConstants.charKdUuid);
      _tankLeftChar = _findCharacteristic(service, SBRConstants.charTankLeftUuid);
      _tankRightChar = _findCharacteristic(service, SBRConstants.charTankRightUuid);
      _controlModeChar = _findCharacteristic(service, SBRConstants.charControlModeUuid);
      _emergencyStopChar = _findCharacteristic(service, SBRConstants.charEmergencyStopUuid);
      _calibrateChar = _findCharacteristic(service, SBRConstants.charCalibrateUuid);
      _telemetryChar = _findCharacteristic(service, SBRConstants.charTelemetryUuid);

      if (_telemetryChar != null) {
        await _telemetryChar!.setNotifyValue(true);
        _telemetryChar!.onValueReceived.listen((value) {
          final data = String.fromCharCodes(value);
          final telemetry = TelemetryData.fromString(data);
          _telemetryController.add(telemetry);
        });
      }

      device.connectionState.listen((state) {
        if (state == BluetoothConnectionState.disconnected) {
          _connectionController.add(false);
        }
      });

      _connectionController.add(true);
    } catch (e) {
      rethrow;
    }
  }

  BluetoothCharacteristic? _findCharacteristic(
      BluetoothService service, String uuid) {
    try {
      return service.characteristics.firstWhere(
        (c) => c.uuid.toString() == uuid,
      );
    } catch (_) {
      return null;
    }
  }

  Future<void> setPID(PIDParams params) async {
    if (!isConnected) return;

    if (_kpChar != null) {
      await _kpChar!.write(params.kp.toString().codeUnits);
    }
    if (_kiChar != null) {
      await _kiChar!.write(params.ki.toString().codeUnits);
    }
    if (_kdChar != null) {
      await _kdChar!.write(params.kd.toString().codeUnits);
    }
  }

  Future<void> setTankControl(TankControl tank) async {
    if (!isConnected) return;

    if (_tankLeftChar != null) {
      await _tankLeftChar!.write(tank.leftMotor.toString().codeUnits);
    }
    if (_tankRightChar != null) {
      await _tankRightChar!.write(tank.rightMotor.toString().codeUnits);
    }
  }

  Future<void> setControlMode(ControlMode mode) async {
    if (!isConnected || _controlModeChar == null) return;
    await _controlModeChar!.write([mode.value]);
  }

  Future<void> triggerEmergencyStop() async {
    if (!isConnected || _emergencyStopChar == null) return;
    await _emergencyStopChar!.write([1]);
  }

  Future<void> triggerCalibration() async {
    if (!isConnected || _calibrateChar == null) return;
    await _calibrateChar!.write([1]);
  }

  Future<void> disconnect() async {
    if (_device != null) {
      await _device!.disconnect();
      _device = null;
    }
  }

  void dispose() {
    disconnect();
    _telemetryController.close();
    _connectionController.close();
  }
}

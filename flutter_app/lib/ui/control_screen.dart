import 'package:flutter/material.dart';
import 'package:fl_chart/fl_chart.dart';
import '../ble/ble_service.dart';
import '../models/models.dart';
import 'connection_screen.dart';

class ControlScreen extends StatefulWidget {
  final BLEService bleService;

  const ControlScreen({super.key, required this.bleService});

  @override
  State<ControlScreen> createState() => _ControlScreenState();
}

class _ControlScreenState extends State<ControlScreen> {
  ControlMode _controlMode = ControlMode.autoMode;
  PIDParams _pidParams = PIDParams(kp: 1.0, ki: 0.0, kd: 1.0);
  TelemetryData _telemetry = TelemetryData();
  double _leftTank = 0.0;
  double _rightTank = 0.0;
  bool _emergencyStopActive = false;

  final List<TelemetryData> _telemetryHistory = [];
  static const int _maxHistorySize = 100;

  @override
  void initState() {
    super.initState();
    _bleService.telemetryStream.listen((data) {
      setState(() {
        _telemetry = data;
        _telemetryHistory.add(data);
        if (_telemetryHistory.length > _maxHistorySize) {
          _telemetryHistory.removeAt(0);
        }
        if (data.emergencyStop) {
          _emergencyStopActive = true;
        }
      });
    });

    _bleService.connectionStream.listen((connected) {
      if (!connected) {
        Navigator.of(context).pushReplacement(
          MaterialPageRoute(builder: (_) => const ConnectionScreen()),
        );
      }
    });
  }

  void _sendTankControl() {
    _bleService.setTankControl(TankControl(
      leftMotor: _leftTank,
      rightMotor: _rightTank,
    ));
  }

  void _setControlMode(ControlMode mode) {
    setState(() {
      _controlMode = mode;
    });
    _bleService.setControlMode(mode);
  }

  void _sendPID() {
    _bleService.setPID(_pidParams);
  }

  void _emergencyStop() {
    _bleService.triggerEmergencyStop();
    setState(() {
      _emergencyStopActive = true;
      _leftTank = 0.0;
      _rightTank = 0.0;
    });
  }

  void _clearEmergencyStop() {
    setState(() {
      _emergencyStopActive = false;
    });
  }

  void _calibrate() {
    _bleService.triggerCalibration();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('SBR Control'),
        backgroundColor: _emergencyStopActive
            ? Colors.red
            : Theme.of(context).colorScheme.inversePrimary,
        actions: [
          IconButton(
            icon: const Icon(Icons.bluetooth_connected),
            onPressed: () {},
            tooltip: 'Connected',
          ),
        ],
      ),
      body: _emergencyStopActive ? _buildEmergencyStopView() : _buildMainView(),
    );
  }

  Widget _buildEmergencyStopView() {
    return Center(
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          const Icon(Icons.warning, size: 100, color: Colors.red),
          const SizedBox(height: 24),
          const Text(
            'EMERGENCY STOP',
            style: TextStyle(fontSize: 32, fontWeight: FontWeight.bold),
          ),
          const SizedBox(height: 8),
          const Text(
            'Motors disabled',
            style: TextStyle(fontSize: 18, color: Colors.grey),
          ),
          const SizedBox(height: 48),
          ElevatedButton.icon(
            onPressed: _clearEmergencyStop,
            icon: const Icon(Icons.play_arrow),
            label: const Text('CLEAR EMERGENCY STOP'),
            style: ElevatedButton.styleFrom(
              backgroundColor: Colors.green,
              foregroundColor: Colors.white,
              padding: const EdgeInsets.symmetric(horizontal: 32, vertical: 16),
            ),
          ),
          const SizedBox(height: 16),
          TextButton(
            onPressed: () {
              _bleService.disconnect();
              Navigator.of(context).pushReplacement(
                MaterialPageRoute(builder: (_) => const ConnectionScreen()),
              );
            },
            child: const Text('Disconnect'),
          ),
        ],
      ),
    );
  }

  Widget _buildMainView() {
    return SingleChildScrollView(
      padding: const EdgeInsets.all(16),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          _buildTelemetryCard(),
          const SizedBox(height: 16),
          _buildControlModeCard(),
          const SizedBox(height: 16),
          _buildTankControlCard(),
          const SizedBox(height: 16),
          _buildPIDTuningCard(),
          const SizedBox(height: 16),
          _buildActionsCard(),
          const SizedBox(height: 16),
          _buildTelemetryChart(),
        ],
      ),
    );
  }

  Widget _buildTelemetryCard() {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                const Text(
                  'Telemetry',
                  style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
                ),
                Chip(
                  label: Text(_controlMode.name),
                  backgroundColor: Colors.blue[100],
                ),
              ],
            ),
            const SizedBox(height: 8),
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceAround,
              children: [
                _buildTelemetryValue('Pitch', _telemetry.pitch.toStringAsFixed(1)),
                _buildTelemetryValue('Roll', _telemetry.roll.toStringAsFixed(1)),
                _buildTelemetryValue('Yaw', _telemetry.yaw.toStringAsFixed(1)),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildTelemetryValue(String label, String value) {
    return Column(
      children: [
        Text(label, style: const TextStyle(color: Colors.grey)),
        Text(
          '$value°',
          style: const TextStyle(fontSize: 24, fontWeight: FontWeight.bold),
        ),
      ],
    );
  }

  Widget _buildControlModeCard() {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text(
              'Control Mode',
              style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 12),
            SegmentedButton<ControlMode>(
              segments: ControlMode.values.map((mode) {
                return ButtonSegment<ControlMode>(
                  value: mode,
                  label: Text(mode.name),
                );
              }).toList(),
              selected: {_controlMode},
              onSelectionChanged: (selection) {
                _setControlMode(selection.first);
              },
            ),
            const SizedBox(height: 8),
            Text(
              _getControlModeDescription(_controlMode),
              style: const TextStyle(color: Colors.grey, fontSize: 12),
            ),
          ],
        ),
      ),
    );
  }

  String _getControlModeDescription(ControlMode mode) {
    switch (mode) {
      case ControlMode.autoMode:
        return 'Pure PID balance control - robot balances automatically';
      case ControlMode.manualMode:
        return 'Tank control only - no PID, manual motor control';
      case ControlMode.mixedMode:
        return 'Tank + PID - manual drive with balance assistance';
    }
  }

  Widget _buildTankControlCard() {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text(
              'Tank Control',
              style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 16),
            Row(
              children: [
                Expanded(
                  child: Column(
                    children: [
                      const Text('Left Motor'),
                      Slider(
                        value: _leftTank,
                        min: -100,
                        max: 100,
                        divisions: 200,
                        label: '${_leftTank.toInt()}%',
                        onChanged: (value) {
                          setState(() {
                            _leftTank = value;
                          });
                          _sendTankControl();
                        },
                      ),
                      Text('${_leftTank.toInt()}%'),
                    ],
                  ),
                ),
                const SizedBox(width: 16),
                Expanded(
                  child: Column(
                    children: [
                      const Text('Right Motor'),
                      Slider(
                        value: _rightTank,
                        min: -100,
                        max: 100,
                        divisions: 200,
                        label: '${_rightTank.toInt()}%',
                        onChanged: (value) {
                          setState(() {
                            _rightTank = value;
                          });
                          _sendTankControl();
                        },
                      ),
                      Text('${_rightTank.toInt()}%'),
                    ],
                  ),
                ),
              ],
            ),
            const SizedBox(height: 8),
            Row(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                ElevatedButton(
                  onPressed: () {
                    setState(() {
                      _leftTank = -50;
                      _rightTank = -50;
                    });
                    _sendTankControl();
                  },
                  child: const Text('◄◄'),
                ),
                const SizedBox(width: 8),
                ElevatedButton(
                  onPressed: () {
                    setState(() {
                      _leftTank = 0;
                      _rightTank = 0;
                    });
                    _sendTankControl();
                  },
                  child: const Text('STOP'),
                ),
                const SizedBox(width: 8),
                ElevatedButton(
                  onPressed: () {
                    setState(() {
                      _leftTank = 50;
                      _rightTank = 50;
                    });
                    _sendTankControl();
                  },
                  child: const Text('►►'),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildPIDTuningCard() {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                const Text(
                  'PID Tuning',
                  style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
                ),
                ElevatedButton(
                  onPressed: _sendPID,
                  child: const Text('Apply'),
                ),
              ],
            ),
            const SizedBox(height: 16),
            _buildPIDSlider('Kp', _pidParams.kp, 0.0, 10.0, (v) {
              setState(() => _pidParams.kp = v);
            }),
            _buildPIDSlider('Ki', _pidParams.ki, 0.0, 1.0, (v) {
              setState(() => _pidParams.ki = v);
            }),
            _buildPIDSlider('Kd', _pidParams.kd, 0.0, 10.0, (v) {
              setState(() => _pidParams.kd = v);
            }),
          ],
        ),
      ),
    );
  }

  Widget _buildPIDSlider(String label, double value, double min, double max, Function(double) onChanged) {
    return Row(
      children: [
        SizedBox(
          width: 40,
          child: Text(label, style: const TextStyle(fontWeight: FontWeight.bold)),
        ),
        Expanded(
          child: Slider(
            value: value,
            min: min,
            max: max,
            onChanged: onChanged,
          ),
        ),
        SizedBox(
          width: 60,
          child: Text(value.toStringAsFixed(3)),
        ),
      ],
    );
  }

  Widget _buildActionsCard() {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            const Text(
              'Actions',
              style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 12),
            Row(
              children: [
                Expanded(
                  child: ElevatedButton.icon(
                    onPressed: _calibrate,
                    icon: const Icon(Icons.tune),
                    label: const Text('Calibrate'),
                    style: ElevatedButton.styleFrom(
                      backgroundColor: Colors.orange,
                      foregroundColor: Colors.white,
                    ),
                  ),
                ),
                const SizedBox(width: 16),
                Expanded(
                  child: ElevatedButton.icon(
                    onPressed: _emergencyStop,
                    icon: const Icon(Icons.warning),
                    label: const Text('E-STOP'),
                    style: ElevatedButton.styleFrom(
                      backgroundColor: Colors.red,
                      foregroundColor: Colors.white,
                    ),
                  ),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildTelemetryChart() {
    if (_telemetryHistory.isEmpty) {
      return const SizedBox.shrink();
    }

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text(
              'Pitch/Roll History',
              style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
            ),
            const SizedBox(height: 16),
            SizedBox(
              height: 200,
              child: LineChart(
                LineChartData(
                  gridData: const FlGridData(show: true),
                  titlesData: const FlTitlesData(
                    leftTitles: AxisTitles(
                      sideTitles: SideTitles(showTitles: true, reservedSize: 40),
                    ),
                    bottomTitles: AxisTitles(
                      sideTitles: SideTitles(showTitles: false),
                    ),
                    topTitles: AxisTitles(
                      sideTitles: SideTitles(showTitles: false),
                    ),
                    rightTitles: AxisTitles(
                      sideTitles: SideTitles(showTitles: false),
                    ),
                  ),
                  lineBarsData: [
                    LineChartBarData(
                      spots: _telemetryHistory
                          .asMap()
                          .entries
                          .map((e) => FlSpot(e.key.toDouble(), e.value.pitch))
                          .toList(),
                      color: Colors.blue,
                      dotData: const FlDotData(show: false),
                    ),
                    LineChartBarData(
                      spots: _telemetryHistory
                          .asMap()
                          .entries
                          .map((e) => FlSpot(e.key.toDouble(), e.value.roll))
                          .toList(),
                      color: Colors.red,
                      dotData: const FlDotData(show: false),
                    ),
                  ],
                ),
              ),
            ),
            const SizedBox(height: 8),
            Row(
              mainAxisAlignment: MainAxisAlignment.center,
              children: const [
                _ChartLegend(color: Colors.blue, label: 'Pitch'),
                SizedBox(width: 16),
                _ChartLegend(color: Colors.red, label: 'Roll'),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

class _ChartLegend extends StatelessWidget {
  final Color color;
  final String label;

  const _ChartLegend({required this.color, required this.label});

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        Container(
          width: 16,
          height: 4,
          color: color,
        ),
        const SizedBox(width: 4),
        Text(label),
      ],
    );
  }
}

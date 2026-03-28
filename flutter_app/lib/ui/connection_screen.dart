import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import '../ble/ble_service.dart';
import '../utils/constants.dart';
import 'control_screen.dart';

class ConnectionScreen extends StatefulWidget {
  const ConnectionScreen({super.key});

  @override
  State<ConnectionScreen> createState() => _ConnectionScreenState();
}

class _ConnectionScreenState extends State<ConnectionScreen> {
  final BLEService _bleService = BLEService();
  bool _isScanning = false;
  bool _isConnecting = false;
  String? _error;

  @override
  void dispose() {
    _bleService.dispose();
    super.dispose();
  }

  Future<void> _scanForDevices() async {
    setState(() {
      _isScanning = true;
      _error = null;
    });

    try {
      await FlutterBluePlus.startScan(
        withServices: [Guid(SBRConstants.serviceUuid)],
        timeout: SBRConstants.scanTimeout,
      );

      await for (var results in FlutterBluePlus.scanResults) {
        if (!mounted) return;
        
        for (var result in results) {
          if (result.device.platformName == SBRConstants.deviceName) {
            await FlutterBluePlus.stopScan();
            await _connectToDevice(result.device);
            return;
          }
        }
      }

      setState(() {
        _isScanning = false;
        _error = 'SBR-Bot not found. Make sure it is powered on.';
      });
    } catch (e) {
      setState(() {
        _isScanning = false;
        _error = 'Error: $e';
      });
    }
  }

  Future<void> _connectToDevice(BluetoothDevice device) async {
    setState(() {
      _isConnecting = true;
      _error = null;
    });

    try {
      await _bleService.connect(device);
      if (mounted) {
        Navigator.of(context).pushReplacement(
          MaterialPageRoute(builder: (_) => ControlScreen(bleService: _bleService)),
        );
      }
    } catch (e) {
      setState(() {
        _isConnecting = false;
        _error = 'Connection failed: $e';
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('SBR Controller'),
        backgroundColor: Theme.of(context).colorScheme.inversePrimary,
      ),
      body: Center(
        child: Padding(
          padding: const EdgeInsets.all(24.0),
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              Icon(
                Icons.bluetooth,
                size: 80,
                color: Theme.of(context).colorScheme.primary,
              ),
              const SizedBox(height: 24),
              Text(
                'Self-Balancing Robot',
                style: Theme.of(context).textTheme.headlineSmall,
              ),
              const SizedBox(height: 8),
              Text(
                'BLE Controller',
                style: Theme.of(context).textTheme.titleMedium?.copyWith(
                      color: Colors.grey[600],
                    ),
              ),
              const SizedBox(height: 48),
              if (_isScanning || _isConnecting)
                const CircularProgressIndicator()
              else
                ElevatedButton.icon(
                  onPressed: _scanForDevices,
                  icon: const Icon(Icons.search),
                  label: const Text('Scan for SBR-Bot'),
                  style: ElevatedButton.styleFrom(
                    padding: const EdgeInsets.symmetric(
                      horizontal: 32,
                      vertical: 16,
                    ),
                  ),
                ),
              if (_error != null) ...[
                const SizedBox(height: 16),
                Text(
                  _error!,
                  style: TextStyle(color: Theme.of(context).colorScheme.error),
                  textAlign: TextAlign.center,
                ),
              ],
              const SizedBox(height: 24),
              StreamBuilder<bool>(
                stream: _bleService.connectionStream,
                builder: (context, snapshot) {
                  if (snapshot.data == true) {
                    return const Text('Connected!');
                  }
                  return const SizedBox.shrink();
                },
              ),
            ],
          ),
        ),
      ),
    );
  }
}

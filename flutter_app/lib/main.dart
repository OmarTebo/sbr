import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'ble/ble_service.dart';
import 'models/models.dart';
import 'ui/connection_screen.dart';
import 'ui/control_screen.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  runApp(const SBRApp());
}

class SBRApp extends StatelessWidget {
  const SBRApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'SBR Controller',
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: Colors.blue),
        useMaterial3: true,
      ),
      home: const ConnectionScreen(),
    );
  }
}

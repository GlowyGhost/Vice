import 'package:flutter/material.dart';
import 'package:vice/invoke_js.dart';
import 'settings/page.dart';
import 'window.dart';

void main() async {
  runApp(const App());
}

class App extends StatefulWidget {
  const App({super.key});

  @override
  State<App> createState() => AppState();
}

class AppState extends State<App> {
  bool _isLoaded = false;

  @override
  void initState() {
    super.initState();
    _loadSettings();
  }

  Future<void> _loadSettings() async {
    try {
      await settings.loadSettings();
    } catch (e) {
      printText("Error loading settings: $e");
    }

    setState(() {_isLoaded = true;});
  }

  @override
  Widget build(BuildContext context) {
    if (!_isLoaded) {
      return const MaterialApp(
        home: Scaffold(
          body: Center(child: CircularProgressIndicator()),
        ),
      );
    }

    return MaterialApp(
      title: "Vice",
      theme: ThemeData(
        scaffoldBackgroundColor: const Color(0xFF262626),
        primaryColor: const Color(0xFF262626),
      ),
      home: const Window(),
    );
  }
}

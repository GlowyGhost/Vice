import 'package:flutter/material.dart';
import 'window.dart';

void main() {
  runApp(const App());
}

class App extends StatelessWidget {
  const App({super.key});

  @override
  Widget build(BuildContext context) {
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

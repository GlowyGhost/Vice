import 'package:flutter/material.dart';

import 'invoke_js.dart';

void main() {
  runApp(const MainApp());
}

class MainApp extends StatelessWidget {
  const MainApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        body: Center(
          child: Row(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              const Text('Hello from Flutter!'),
              const SizedBox(width: 10),
              TextButton(onPressed: () => invokeJS("greet", const {"name": "Flutter"}), child: const Text('Greet from Rust'))
            ],
          ),
        ),
      ),
    );
  }
}

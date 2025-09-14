import 'package:flutter/material.dart';

enum WindowPage { channels, soundboard }

class Window extends StatefulWidget {
  const Window({super.key});

  @override
  State<Window> createState() => _WindowState();
}

class _WindowState extends State<Window> {
  WindowPage _currentPage = WindowPage.channels;

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: const Color(0xFF262626),
      body: Column(
        children: [
          Align(
            alignment: Alignment.topCenter,
            child: Container(
              width: MediaQuery.of(context).size.width,
              height: 70,
              decoration: BoxDecoration(
                color: const Color(0xFF1F1F1F),
                borderRadius: const BorderRadius.only(
                  bottomLeft: Radius.circular(36),
                  bottomRight: Radius.circular(36),
                ),
              ),
              child: Row(
                mainAxisAlignment: MainAxisAlignment.start,
                children: [
                  TextButton(
                    style: TextButton.styleFrom(
                      foregroundColor: _currentPage == WindowPage.channels ? 
                        Color(0xFFFFFFFF)
                        : Color(0xFFB3B3B3),
                      padding: const EdgeInsets.symmetric(vertical: 20, horizontal: 20),
                      alignment: Alignment.centerLeft,
                    ),
                    onPressed: () => setState(() => _currentPage = WindowPage.channels),
                    child: Text("Channels", style: TextStyle(fontSize: 32)),
                  ),

                  const SizedBox(height: 10),

                  TextButton(
                    style: TextButton.styleFrom(
                      foregroundColor: _currentPage == WindowPage.soundboard ? 
                        Color(0xFFFFFFFF)
                        : Color(0xFFB3B3B3),
                      padding: const EdgeInsets.symmetric(vertical: 20, horizontal: 20),
                      alignment: Alignment.centerLeft,
                    ),
                    onPressed: () => setState(() => _currentPage = WindowPage.soundboard),
                    child: Text("Soundboard", style: TextStyle(fontSize: 32)),
                  ),
                ],
              ),
            )
          ),

          Expanded(
            child: Container(
              color: const Color(0xFF262626),
              child: switch (_currentPage) {
                WindowPage.channels => const Center(child: Text('Channels Page', style: TextStyle(color: Colors.white))),
                WindowPage.soundboard => const Center(child: Text('Soundboard Page', style: TextStyle(color: Colors.white))),
              }
            )
          )
        ]
      )
    );
  }
}
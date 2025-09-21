import 'package:flutter/material.dart';
import 'invoke_js.dart';
import 'soundboard/page.dart';
import 'channels/page.dart';

enum WindowPage { channels, soundboard }

class Window extends StatefulWidget {
  const Window({super.key});

  @override
  State<Window> createState() => _WindowState();
}

class _WindowState extends State<Window> {
  WindowPage _currentPage = WindowPage.channels;
  String outputDevice = "Please wait.";
  List<String> devices = ["Please wait."];

  @override
  void initState() {
    super.initState();

    _init();
  }

  Future<void> _init() async {
    final String? device = await invokeJS("get_output");
    final outputs = await invokeJS("get_outputs");

    setState(() {devices = outputs;});

    if (device != null && device.trim().isNotEmpty) {
      setState(() {outputDevice = device;});
    } else {
      setState(() {outputDevice = "Select your output device.";});
    }
  }

  Future<void> _save() async {
    await invokeJS("set_output", {"output": outputDevice});
  }

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

                  Spacer(),

                  TextButton(
                    onPressed: () {
                      showDialog(
                        context: context,
                        builder: (context) => DeviceDropdown(
                          devices: devices,
                          onDeviceSelected: (optionSelected) {
                            setState(() {outputDevice = optionSelected;});
                            _save();
                          },
                        ),
                      );
                    },
                    child: Text(outputDevice, style: TextStyle(fontSize: 30, color: Colors.white))
                  ),
                ],
              ),
            )
          ),

          Expanded(
            child: Container(
              color: const Color(0xFF262626),
              child: switch (_currentPage) {
                WindowPage.channels => ChannelsManagerDisplay(),
                WindowPage.soundboard => SoundboardManagerDisplay(),
              }
            )
          )
        ]
      )
    );
  }
}
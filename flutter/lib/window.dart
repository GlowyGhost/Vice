import 'package:flutter/material.dart';
import 'invoke_js.dart';
import 'settings/page.dart';
import 'soundboard/page.dart';
import 'channels/page.dart';
import 'package:font_awesome_flutter/font_awesome_flutter.dart';

enum WindowPage { channels, soundboard, settings }

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
              width: context.size?.width == null ? MediaQuery.of(context).size.width : context.size!.width,
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

                  const SizedBox(height: 10),

                  TextButton(
                    style: TextButton.styleFrom(
                      foregroundColor: _currentPage == WindowPage.settings ? 
                        Color(0xFFFFFFFF)
                        : Color(0xFFB3B3B3),
                      padding: const EdgeInsets.symmetric(vertical: 20, horizontal: 20),
                      alignment: Alignment.centerLeft,
                    ),
                    onPressed: () => setState(() => _currentPage = WindowPage.settings),
                    child: Text("Settings", style: TextStyle(fontSize: 32)),
                  ),

                  Spacer(),

                  Padding(
                    padding: EdgeInsets.all(4),
                    child: Column(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        Text(
                          "Version: ${settings.version}",
                          style: TextStyle(
                            fontSize: 16,
                            color: Color(0xFFFFFFFF)
                          ),
                        ),
                        Row(
                          mainAxisSize: MainAxisSize.min,
                          mainAxisAlignment: MainAxisAlignment.center,
                          children: [
                            IconButton(
                              icon: Icon(FontAwesomeIcons.github, color: Color(0xFFFFFFFF)),
                              onPressed: () async {
                                await invokeJS("open_link", {"url": "https://github.com/GlowyGhost/Vice"});
                              },
                            )
                          ],
                        )
                      ],
                    )
                  )
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
                WindowPage.settings => SettingsPage()
              }
            )
          )
        ]
      )
    );
  }
}
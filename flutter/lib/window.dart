import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:vice/blocks/page.dart';
import 'package:vice/main.dart';
import 'package:vice/randoms.dart';
import 'invoke_js.dart';
import 'performance/page.dart';
import 'settings/page.dart';
import 'soundboard/page.dart';
import 'channels/page.dart';
import 'package:font_awesome_flutter/font_awesome_flutter.dart';

enum WindowPage { Channels, Soundboard, Settings, Performance, Blocks }

class Window extends StatefulWidget {
  const Window({super.key});

  @override
  State<Window> createState() => _WindowState();
}

class _WindowState extends State<Window> {
  WindowPage _currentPage = WindowPage.Channels;

  @override
  Widget build(BuildContext context) {
    context.watch<AppStateNotifier>();
    
    return Scaffold(
      body: Column(
        children: [
          Align(
            alignment: Alignment.topCenter,
            child: Container(
              width: MediaQuery.of(context).size.width,
              height: 70,
              decoration: BoxDecoration(
                color: bg_mid,
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
                      foregroundColor: _currentPage == WindowPage.Channels ? text : text_muted,
                      padding: const EdgeInsets.symmetric(vertical: 20, horizontal: 20),
                      alignment: Alignment.centerLeft,
                    ),
                    onPressed: () => setState(() => _currentPage = WindowPage.Channels),
                    child: Text("Channels", style: TextStyle(fontSize: 32)),
                  ),

                  const SizedBox(height: 10),

                  TextButton(
                    style: TextButton.styleFrom(
                      foregroundColor: _currentPage == WindowPage.Soundboard ? text : text_muted,
                      padding: const EdgeInsets.symmetric(vertical: 20, horizontal: 20),
                      alignment: Alignment.centerLeft,
                    ),
                    onPressed: () => setState(() => _currentPage = WindowPage.Soundboard),
                    child: Text("Soundboard", style: TextStyle(fontSize: 32)),
                  ),

                  const SizedBox(height: 10),

                  TextButton(
                    style: TextButton.styleFrom(
                      foregroundColor: _currentPage == WindowPage.Settings ? text : text_muted,
                      padding: const EdgeInsets.symmetric(vertical: 20, horizontal: 20),
                      alignment: Alignment.centerLeft,
                    ),
                    onPressed: () => setState(() => _currentPage = WindowPage.Settings),
                    child: Text("Settings", style: TextStyle(fontSize: 32)),
                  ),

                  const SizedBox(height: 10),

                  performance(),

                  const SizedBox(height: 10),

                  TextButton(
                    style: TextButton.styleFrom(
                      foregroundColor: _currentPage == WindowPage.Blocks ? text : text_muted,
                      padding: const EdgeInsets.symmetric(vertical: 20, horizontal: 20),
                      alignment: Alignment.centerLeft,
                    ),
                    onPressed: () => setState(() => _currentPage = WindowPage.Blocks),
                    child: Text("Blocks", style: TextStyle(fontSize: 32)),
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
                            color: text
                          ),
                        ),
                        Row(
                          mainAxisSize: MainAxisSize.min,
                          mainAxisAlignment: MainAxisAlignment.center,
                          children: [
                            IconButton(
                              icon: Icon(FontAwesomeIcons.github, color: text),
                              onPressed: () async {
                                await invokeJS("open_link", {"url": "https://github.com/GlowyDeveloper/Vice"});
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
              color: bg_dark,
              child: switch (_currentPage) {
                WindowPage.Channels => ChannelsManagerDisplay(),
                WindowPage.Soundboard => SoundboardManagerDisplay(),
                WindowPage.Settings => SettingsPage(),
                WindowPage.Performance => PerformancePage(),
                WindowPage.Blocks => BlocksManagerDisplay()
              }
            )
          )
        ]
      )
    );
  }

  Widget performance() {
    if (settings.monitor == true) {
      return TextButton(
        style: TextButton.styleFrom(
          foregroundColor: _currentPage == WindowPage.Performance ? text : text_muted,
          padding: const EdgeInsets.symmetric(vertical: 20, horizontal: 20),
          alignment: Alignment.centerLeft,
        ),
        onPressed: () => setState(() => _currentPage = WindowPage.Performance),
        child: Text("Performance", style: TextStyle(fontSize: 32)),
      );
    }
    return Center();
  }
}
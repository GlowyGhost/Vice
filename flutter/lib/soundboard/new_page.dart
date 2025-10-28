import 'package:flutter/material.dart';
import '../icons.dart';
import '../invoke_js.dart';
import '../settings/page.dart';
import 'main_page.dart';
import 'page.dart';
import 'package:flutter_colorpicker/flutter_colorpicker.dart';

class SoundboardNew extends StatefulWidget {
  const SoundboardNew({super.key});

  @override
  State<SoundboardNew> createState() => _SoundboardNewState();
}

class _SoundboardNewState extends State<SoundboardNew> {
  Color pickerColor = Color(0xFFFF0000);
  Color currentColor = Color(0xFFFF0000);
  String icon = "question_mark";
  final TextEditingController controllerName = TextEditingController();
  final TextEditingController controllerSound = TextEditingController();
  final ScrollController scrollController = ScrollController();
  bool lowlatency = false;
  
  void changeColor(Color color) {
    setState(() => pickerColor = color);
  }

  Future<void> _save() async {
    await invokeJS("new_sound", {
      "color": [currentColor.r*255.toInt(), currentColor.g*255.toInt(), currentColor.b*255.toInt()],
      "icon": icon,
      "name": controllerName.text,
      "low": lowlatency,
      "sound": controllerSound.text});
    
    SoundboardPageClass.setPage(SoundboardMain());
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: Scrollbar(
        controller: scrollController,
        thumbVisibility: true,
        thickness: 12,
        radius: const Radius.circular(12),
        trackVisibility: false,
        interactive: true,
        child: SingleChildScrollView(
          controller: scrollController,
          padding: EdgeInsetsGeometry.all(8),
          child: Column(
            children: [
              Row(
                children: [
                  IconButton(
                    icon: const Icon(Icons.format_paint),
                    color: currentColor,
                    iconSize: 96,
                    onPressed: () {
                      showDialog(
                        context: context,
                        builder: (context) => AlertDialog(
                          backgroundColor: const Color(0xFF363636),
                          title: const Text("Choose a color", style: TextStyle(color: Colors.white)),
                          content: SingleChildScrollView(
                            child: Theme(
                              data: Theme.of(context).copyWith(
                                canvasColor: const Color(0xFF363636),
                                popupMenuTheme: const PopupMenuThemeData(color: Color(0xFF363636), textStyle: TextStyle(color: Colors.white)),
                                textTheme: Theme.of(context).textTheme.apply(bodyColor: Colors.white, displayColor: Colors.white),
                                primaryTextTheme: Theme.of(context).primaryTextTheme.apply(bodyColor: Colors.white, displayColor: Colors.white),
                              ),
                              child: ColorPicker(
                                pickerColor: pickerColor,
                                onColorChanged: changeColor,
                                enableAlpha: false,
                                showLabel: true,
                                pickerAreaHeightPercent: 0.8,
                                labelTextStyle: const TextStyle(color: Colors.white),
                              ),
                            ),
                          ),

                          actions: <Widget>[
                            TextButton(
                              child: const Text("Cancel", style: TextStyle(color: Colors.white)),
                              onPressed: () => Navigator.of(context).pop(),
                            ),
                            ElevatedButton(
                              child: const Text("OK"),
                              onPressed: () {
                                setState(() => currentColor = pickerColor);
                                Navigator.of(context).pop();
                              },
                            ),
                          ],
                        ),
                      );
                    },
                  ),

                  const SizedBox(width: 16),

                  Expanded(
                    child: TextField(
                      controller: controllerName,
                      style: TextStyle(color: settings.lightMode ? Color(0xFF000000) : Color(0xFFFFFFFF)),
                      decoration: InputDecoration(
                        border: const OutlineInputBorder(),
                        labelText: "Enter name",
                        labelStyle: TextStyle(color: settings.lightMode ? Color(0xFF000000) : Color(0xFFFFFFFF)),
                        filled: true,
                        fillColor: settings.lightMode ? const Color(0xFF999999) : const Color(0xFF363636),
                      ),
                    ),
                  ),
                ],
              ),

              const SizedBox(height: 20),

              Row(
                children: [
                  IconButton(
                    icon: Icon(getIcon(icon)),
                    color: settings.lightMode ? Color(0xFF000000) : Color(0xFFFFFFFF),
                    iconSize: 96,
                    onPressed: () {
                      showDialog(
                        context: context,
                        builder: (context) => IconGridDropdown(
                          icons: IconMap.values.toList(),
                          onIconSelected: (iconSelected) {
                            setState(() {icon = fromIcon(iconSelected);});
                          },
                        ),
                      );
                    },
                  ),

                  const SizedBox(width: 35),

                  Expanded(
                    child: Row(
                      children: [
                        Expanded(
                          child: TextField(
                            controller: controllerSound,
                            style: TextStyle(color: settings.lightMode ? Color(0xFF000000) : Color(0xFFFFFFFF)),
                            decoration: InputDecoration(
                              border: const OutlineInputBorder(),
                              labelText: "Enter Sound Effect",
                              labelStyle: TextStyle(color: settings.lightMode ? Color(0xFF000000) : Color(0xFFFFFFFF)),
                              filled: true,
                              fillColor: settings.lightMode ? const Color(0xFF999999) : const Color(0xFF363636),
                            ),
                          )
                        ),

                        const SizedBox(width: 16),

                        IconButton(
                          onPressed: () async => {controllerSound.text = await invokeJS("pick_menu_sound")},
                          icon: Icon(Icons.menu, color: settings.lightMode ? Color(0xFF000000) : Color(0xFFFFFFFF), size: 96)
                        )
                      ],
                    )
                  ),
                ],
              ),

              const SizedBox(height: 20),

              SwitchListTile(
                title: Text("Low latency mode", style: TextStyle(fontSize: 18, color: settings.lightMode ? Color(0xFF000000) : Color(0xFFFFFFFF))),
                value: lowlatency,
                onChanged: (value) {
                  setState(() => lowlatency = value);
                },
              ),
            ],
          ),
        ),
      ),
    
      bottomNavigationBar: Padding(
				padding: const EdgeInsets.all(16),
				child: Row(
					children: [
						ElevatedButton.icon(
              style: TextButton.styleFrom(
                backgroundColor: settings.lightMode ? const Color(0xFF262626) : Color(0xFFCCCCCC),
                foregroundColor: Colors.purpleAccent
              ),
							onPressed: _save,
							icon: const Icon(Icons.save),
							label: Text("Save", style: TextStyle(fontSize: 18))
						),

            const SizedBox(width: 16),

            ElevatedButton.icon(
              style: TextButton.styleFrom(
                backgroundColor: settings.lightMode ? const Color(0xFF262626) : Color(0xFFCCCCCC),
                foregroundColor: Colors.purpleAccent
              ),
							onPressed: () => {SoundboardPageClass.setPage(SoundboardMain())},
							icon: const Icon(Icons.undo_rounded),
							label: Text("Back", style: TextStyle(fontSize: 18))
						),
					],
				)
			),
    );
  }
}
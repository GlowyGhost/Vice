import 'package:flutter/material.dart';
import '../icons.dart';
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
  
  void changeColor(Color color) {
    setState(() => pickerColor = color);
  }

  @override
  Widget build(BuildContext context) {
    final TextEditingController controllerName = TextEditingController();
    final TextEditingController controllerSound = TextEditingController();

    return Scaffold(
      body: Padding(
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
                    style: TextStyle(color: Colors.white),
                    decoration: InputDecoration(
                      border: const OutlineInputBorder(),
                      labelText: "Enter name",
                      labelStyle: TextStyle(color: Colors.white),
                      filled: true,
                      fillColor: Color(0xFF363636),
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
                  color: Colors.white,
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
                          style: TextStyle(color: Colors.white),
                          decoration: InputDecoration(
                            border: const OutlineInputBorder(),
                            labelText: "Enter Sound Effect",
                            labelStyle: TextStyle(color: Colors.white),
                            filled: true,
                            fillColor: Color(0xFF363636),
                          ),
                        )
                      ),

                      const SizedBox(width: 16),

                      IconButton(
                        onPressed: () => {controllerSound.text = "In progress"}, //TODO: Use rust to open dialog.
                        icon: Icon(Icons.menu, color: Colors.white, size: 96)
                      )
                    ],
                  )
                ),
              ],
            ),
          ],
        ),
      ),
    
      bottomNavigationBar: Padding(
				padding: const EdgeInsets.all(16),
				child: Row(
					children: [
						ElevatedButton.icon(
							onPressed: () => {print("Saving")}, //TODO: Actually Save
							icon: const Icon(Icons.save),
							label: Text("Save", style: TextStyle(fontSize: 18))
						),

            const SizedBox(width: 16),

            ElevatedButton.icon(
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
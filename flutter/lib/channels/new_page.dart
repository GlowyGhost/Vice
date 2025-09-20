import 'package:flutter/material.dart';
import '../icons.dart';
import 'main_page.dart';
import 'page.dart';
import 'package:flutter_colorpicker/flutter_colorpicker.dart';

class ChannelsNew extends StatefulWidget {
  const ChannelsNew({super.key});

  @override
  State<ChannelsNew> createState() => _ChannelsNewState();
}

class _ChannelsNewState extends State<ChannelsNew> {
  Color pickerColor = Color(0xFFFF0000);
  Color currentColor = Color(0xFFFF0000);
  String icon = "question_mark";
  List<String> Devices = ["Audio 1", "Audio 2"]; //TODO: Get rust to actually get all output devices.
  String selectedDevice = "Select audio device";
  
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
                  child: TextButton(
                    onPressed: () {
                      showDialog(
                        context: context,
                        builder: (context) => DeviceDropdown(
                          devices: Devices,
                          onDeviceSelected: (deviceSelected) {
                            setState(() {selectedDevice = deviceSelected;});
                          },
                        ),
                      );
                    },
                    child: Text(selectedDevice, style: TextStyle(fontSize: 30, color: Colors.white))
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
							onPressed: () => {ChannelsPageClass.setPage(ChannelsMain())},
							icon: const Icon(Icons.undo_rounded),
							label: Text("Back", style: TextStyle(fontSize: 18))
						),
					],
				)
			),
    );
  }
}

class DeviceDropdown extends StatelessWidget {
  final List<String> devices;
  final ValueChanged<String> onDeviceSelected;

  DeviceDropdown({required this.devices, required this.onDeviceSelected});

  @override
  Widget build(BuildContext context) {
    return Dialog(
      backgroundColor: Color(0xFF363636),
      insetPadding: EdgeInsets.all(20),
      child: Container(
        width: double.maxFinite,
        height: 300,
        padding: EdgeInsets.all(16),
        child: ListView.builder(
          itemCount: devices.length,
          /*gridDelegate: SliverGridDelegateWithFixedCrossAxisCount(
            crossAxisCount: 10,
            mainAxisSpacing: 16,
            crossAxisSpacing: 16,
          ),*/
          itemBuilder: (context, index) {
            return InkWell(
              onTap: () {
                onDeviceSelected(devices[index]);
                Navigator.pop(context);
              },
              child: Padding(
                padding: EdgeInsets.all(8),
                child: Container(
                  decoration: BoxDecoration(
                    color: Color(0xFF3F3F3F),
                    borderRadius: BorderRadius.circular(8),
                  ),
                  child: Text(devices[index], style: TextStyle(fontSize: 30, color: Colors.white)),
                ),
              )
            );
          },
        ),
      ),
    );
  }
}
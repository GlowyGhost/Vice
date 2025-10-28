import 'package:flutter/material.dart';
import 'package:vice/invoke_js.dart';
import '../icons.dart';
import '../settings/page.dart';
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
  List<String> devicesApps = ["Reopen this menu.", "If this persists,", "check if you have audio devices connected if you're on option \"Capture device\".", "If you're on \"Capture app\", restart your computer."];
  String selectedDeviceApp = "Select audio device";
  String deviceOrApp = "Select audio device";
  bool device = true;
  bool lowlatency = false;
  final TextEditingController controllerName = TextEditingController();
  final ScrollController scrollController = ScrollController();
  
  @override
  void initState() {
    super.initState();

    _init();
  }

  Future<void> _init() async {
    final result = device
      ? await invokeJS("get_devices")
      : await invokeJS("get_apps");

    setState(() {devicesApps = result;});
  }

  void changeColor(Color color) {
    setState(() => pickerColor = color);
  }

  Future<void> _save() async {
    await invokeJS("new_channel", {
      "color": [currentColor.r*255.toInt(), currentColor.g*255.toInt(), currentColor.b*255.toInt()],
      "icon": icon,
      "name": controllerName.text,
      "deviceapps": selectedDeviceApp,
      "device": device,
      "low": lowlatency});

    ChannelsPageClass.setPage(ChannelsMain());
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
                    child: TextButton(
                      onPressed: () {
                        showDialog(
                          context: context,
                          builder: (context) => DeviceDropdown(
                            devices: devicesApps,
                            onDeviceSelected: (deviceSelected) {
                              setState(() {selectedDeviceApp = deviceSelected;});
                            },
                          ),
                        );
                      },
                      child: Text(selectedDeviceApp, style: TextStyle(fontSize: 30, color: settings.lightMode ? Color(0xFF000000) : Color(0xFFFFFFFF)))
                    )
                  ),
                ],
              ),

              Expanded(
                child: TextButton(
                  onPressed: () {
                    showDialog(
                      context: context,
                      builder: (context) => DeviceDropdown(
                        devices: ["Capture device", "Capture app"],
                        onDeviceSelected: (optionSelected) {
                          optionSelected == "Capture device"
                            ? device = true
                            : device = false;
                          _init();
                        },
                      ),
                    );
                  },
                  child: Text(device == true ? "Capture device" : "Capture App", style: TextStyle(fontSize: 30, color: settings.lightMode ? Color(0xFF000000) : Color(0xFFFFFFFF)))
                )
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
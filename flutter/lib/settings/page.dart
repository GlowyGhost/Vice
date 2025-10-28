import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../channels/page.dart';
import '../invoke_js.dart';
import '../main.dart';

class SettingsPage extends StatefulWidget {
  const SettingsPage({super.key});

  @override
  State<SettingsPage> createState() => _SettingsPageState();
}

class _SettingsPageState extends State<SettingsPage> {
  final ScrollController scrollController = ScrollController();
  String outputDevice = "Please wait";
  double scale = 2147483647.0;
  bool lightMode = false;

  @override
  void initState() {
    super.initState();
    _init();
  }

  Future<void> _init() async {
		await settings.loadSettings();

    setState(() {
      outputDevice = settings.outputDevice;
      scale = settings.scale;
      lightMode = settings.lightMode;
    });
	}

  Future<void> _save() async {
    setState(() {
      settings.outputDevice = outputDevice;
      settings.scale = scale;
      settings.lightMode = lightMode;
    });

    await settings.saveSettings(context);
  }

  void showBar(String text) {
		ScaffoldMessenger.of(context).showSnackBar(
			SnackBar(content: Text(text)),
		);
	}

  Future<void> _uninstall() async {
    String res = await invokeJS("uninstall");

    if (res == "Undid") {
      showBar("Cancelled Uninstall");
    }
  }

  Future<void> _update() async {
    String res = await invokeJS("update");

    if (res == "Undid") {
      showBar("Cancelled Update.");
    } else if (res == "No Update") {
      showBar("There currently is no new availiable update.");
    } else if (res == "No Internet") {
      showBar("No Internet Connection.");
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: Column(
        children: [
          Padding(
            padding: EdgeInsets.only(top: 12, left: 8, bottom: 4),
            child: Row(
              children: [
                ElevatedButton.icon(
                  style: TextButton.styleFrom(
                    backgroundColor: settings.lightMode ? const Color(0xFF262626) : Color(0xFFCCCCCC),
                    foregroundColor: Colors.purpleAccent
                  ),
                  onPressed: _save,
                  icon: const Icon(Icons.save),
                  label: Text("Save")
                )
              ],
            )
          ),

          Expanded(
            child: Scaffold(
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
                          Text("Output:    ", style: TextStyle(fontSize: 30, color: Colors.grey)),
                          Expanded(
                            child: TextButton(
                              onPressed: () {
                                showDialog(
                                  context: context,
                                  builder: (context) => DeviceDropdown(
                                    devices: settings.devices,
                                    onDeviceSelected: (optionSelected) {
                                      setState(() {outputDevice = optionSelected;});
                                    },
                                  ),
                                );
                              },
                              child: Text(outputDevice, style: TextStyle(fontSize: 30, color: settings.lightMode ? Color(0xFF000000) : Color(0xFFFFFFFF)))
                            ),
                          )
                        ],
                      ),

                      const SizedBox(height: 10),

                      Row(
                        children: [
                          Text("Scale:    ", style: TextStyle(fontSize: 30, color: Colors.grey)),
                          Expanded(
                            child: Slider(
                              value: scale,
                              min: 0.1,
                              max: 2.0,
                              divisions: 19,
                              label: scale.toStringAsFixed(1),
                              onChanged: (newScale) {
                                setState(() {scale = newScale;});
                              }
                            )
                          )
                        ],
                      ),

                      const SizedBox(height: 10),

                      SwitchListTile(
                        title: Text("Light mode:     ", style: TextStyle(fontSize: 18, color: settings.lightMode ? Color(0xFF000000) : Color(0xFFFFFFFF))),
                        value: lightMode,
                        onChanged: (value) {
                          setState(() => lightMode = value);
                        },
                      ),
                    ],
                  ),
                ),
              ),
            ),
          )
        ],
      ),

      bottomNavigationBar: Padding(
        padding: EdgeInsetsGeometry.all(16),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            TextButton(
              onPressed: () => _update(),
              child: Text("Update", style: TextStyle(fontSize: 24, color: settings.lightMode ? Color(0xFF000000) : Color(0xFFFFFFFF))),
            ),

            const SizedBox(height: 20),

            TextButton(
              onPressed: () => _uninstall(),
              child: Text("Uninstall", style: TextStyle(fontSize: 24, fontWeight: FontWeight.bold, color: Colors.redAccent)),
            ),
          ],
        )
      ),
    );
  }
}

class SettingsData extends ChangeNotifier {
	String outputDevice = "Please wait";
  List<String> devices = ["Please wait"];
  double scale = 2147483647.0;
  String version = "Please wait";
  bool lightMode = false;

	Future<void> loadSettings() async {
		final settings = await invokeJS('get_settings');
		
    scale = settings["scale"];
    lightMode = settings["light"];
    if (settings["output"] != null && settings["output"].trim().isNotEmpty) {
      outputDevice = settings["output"];
    }

    final outputs = await invokeJS("get_outputs");

    devices = outputs;

    getVersion();
	}

  Future<void> getVersion() async {
    final version = await invokeJS('get_version');
    settings.version = version;
  }

	Future<void> saveSettings(BuildContext context) async {
		await invokeJS("save_settings", {"output": outputDevice, "scale": scale, "light": lightMode});

    final appState = Provider.of<AppStateNotifier>(context, listen: false);
    appState.reload();
	}
}

final settings = SettingsData();
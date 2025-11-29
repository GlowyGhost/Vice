import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:vice/randoms.dart';
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
  bool monitor = false;
  bool peaks = true;
  bool startup = false;

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
      monitor = settings.monitor;
      peaks = settings.peaks;
      startup = settings.startup;
    });
	}

  Future<void> _save() async {
    setState(() {
      settings.outputDevice = outputDevice;
      settings.scale = scale;
      settings.lightMode = lightMode;
      settings.monitor = monitor;
      settings.peaks = peaks;
      settings.startup = startup;
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
    await invokeJS("update");
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
                    backgroundColor: bg_light,
                    foregroundColor: accent
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
                          Text("   Output:", style: TextStyle(fontSize: 18, color: text)),
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
                              child: Text(outputDevice, style: TextStyle(fontSize: 30, color: text))
                            ),
                          )
                        ],
                      ),

                      const SizedBox(height: 10),

                      SwitchListTile(
                        title: Text("Light mode:     ", style: TextStyle(fontSize: 18, color: text)),
                        value: lightMode,
                        activeColor: accent,
                        onChanged: (value) {
                          setState(() => lightMode = value);
                        },
                      ),

                      const SizedBox(height: 10),

                      Row(
                        children: [
                          Text("   Scale:", style: TextStyle(fontSize: 18, color: text)),
                          Expanded(
                            child: Slider(
                              value: scale,
                              min: 0.1,
                              max: 2.0,
                              divisions: 19,
                              activeColor: accent,
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
                        title: Text("Monitor performance:", style: TextStyle(fontSize: 18, color: text)),
                        value: monitor,
                        activeColor: accent,
                        onChanged: (value) {
                          setState(() => monitor = value);
                        },
                      ),

                      const SizedBox(height: 10),

                      SwitchListTile(
                        title: Text("Display channel peaks:", style: TextStyle(fontSize: 18, color: text)),
                        value: peaks,
                        activeColor: accent,
                        onChanged: (value) {
                          setState(() => peaks = value);
                        },
                      ),

                      const SizedBox(height: 10),

                      SwitchListTile(
                        title: Text("Open on startup:", style: TextStyle(fontSize: 18, color: text)),
                        value: startup,
                        activeColor: accent,
                        onChanged: (value) {
                          setState(() => startup = value);
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
              child: Text("Update", style: TextStyle(fontSize: 24, color: text)),
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
  bool monitor = false;
  bool peaks = true;
  bool startup = false;

	Future<void> loadSettings() async {
		final settings = await invokeJS('get_settings');
		
    scale = settings["scale"];
    lightMode = settings["light"];
    monitor = settings["monitor"];
    peaks = settings["peaks"];
    startup = settings["startup"];
    if (settings["output"] != null && settings["output"].trim().isNotEmpty) {
      outputDevice = settings["output"];
    }

    final outputs = await invokeJS("get_outputs");

    devices = outputs;

    getVersion();
	}

  Future<void> getVersion() async {
    final version = await invokeJS("get_version");
    settings.version = version;
  }

	Future<void> saveSettings(BuildContext context) async {
		await invokeJS("save_settings", {"output": outputDevice, "scale": scale, "light": lightMode, "monitor": monitor, "peaks": peaks, "startup": startup});

    notifyListeners();

    final appState = Provider.of<AppStateNotifier>(context, listen: false);
    appState.reload();
	}
}

final settings = SettingsData();
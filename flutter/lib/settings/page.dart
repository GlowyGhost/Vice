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

  @override
  void initState() {
    super.initState();
    _init();
  }

  Future<void> _init() async {
		await settings.loadSettings();
	}

  Future<void> _save() async {
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
            padding: const EdgeInsets.fromLTRB(10, 10, 10, 5),
            child: Row(
              children: [
                ElevatedButton.icon(
                  onPressed: _save,
                  icon: const Icon(Icons.save),
                  label: Text("Save", style: TextStyle(fontSize: 18))
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
                                      setState(() {settings.outputDevice = optionSelected;});
                                    },
                                  ),
                                );
                              },
                              child: Text(settings.outputDevice, style: TextStyle(fontSize: 30, color: Colors.white))
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
                              value: settings.scale,
                              min: 0.1,
                              max: 2.0,
                              divisions: 19,
                              label: settings.scale.toStringAsFixed(1),
                              onChanged: (newScale) {
                                setState(() {settings.scale = newScale;});
                              }
                            )
                          )
                        ],
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
              child: Text("Update", style: TextStyle(fontSize: 24, color: Color(0xFFFFFFFF))),
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

	Future<void> loadSettings() async {
		final settings = await invokeJS('get_settings');
		
    scale = settings["scale"];
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
		await invokeJS("save_settings", {"output": outputDevice, "scale": scale});

    final appState = Provider.of<AppStateNotifier>(context, listen: false);
    appState.reload();
	}
}

final settings = SettingsData();
import 'package:flutter/material.dart';
import '../randoms.dart';
import 'edit_page.dart';
import 'main_page.dart';
import 'new_page.dart';

class ChannelsManagerDisplay extends StatefulWidget {
  const ChannelsManagerDisplay({super.key});

  @override
  State<ChannelsManagerDisplay> createState() => _ChannelsManagerState();
}

class _ChannelsManagerState extends State<ChannelsManagerDisplay> {
  @override
  Widget build(BuildContext context) {
    return AnimatedBuilder(
      animation: ChannelsPageClass,
      builder: (context, _) {
        return Column(
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
                    onPressed: () => {ChannelsPageClass.setPage(ChannelsNew())},
                    icon: const Icon(Icons.add),
                    label: Text("Add"),
                  ),
                ],
              ),
            ),

            Expanded(
              child: ChannelsPageClass.page as Widget
            )
          ],
        );
      }
    );
  }
}

class ChannelsPageHolder extends ChangeNotifier {
  Object? page = ChannelsMain();

  void setPage(Object? newPage) {
    if (newPage is ChannelsMain || newPage is ChannelsEdit || newPage is ChannelsNew) {
      page = newPage;
      notifyListeners();
    }
  }
}

ChannelsPageHolder ChannelsPageClass = ChannelsPageHolder();

class DeviceDropdown extends StatelessWidget {
  final List<String> devices;
  final ValueChanged<String> onDeviceSelected;

  const DeviceDropdown({super.key, required this.devices, required this.onDeviceSelected});

  @override
  Widget build(BuildContext context) {
    return Dialog(
      backgroundColor: bg_mid,
      insetPadding: EdgeInsets.all(20),
      child: Container(
        width: double.maxFinite,
        height: 300,
        padding: EdgeInsets.all(16),
        child: ListView.builder(
          itemCount: devices.length,
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
                    color: bg_light,
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
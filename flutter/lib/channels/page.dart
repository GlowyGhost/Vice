import 'package:flutter/material.dart';
import 'edit_page.dart';
import 'main_page.dart';

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
                    onPressed: () => {print("Add pressed for channels")},
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
    if (newPage is ChannelsMain || newPage is ChannelsEdit) {
      page = newPage;
      notifyListeners();
    }
  }
}

ChannelsPageHolder ChannelsPageClass = ChannelsPageHolder();
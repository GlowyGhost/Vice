import 'package:flutter/material.dart';
import 'package:vice/blocks/view.dart';
import 'package:vice/invoke_js.dart';
import '../randoms.dart';

class BlocksManagerDisplay extends StatefulWidget {
  const BlocksManagerDisplay({super.key});

  @override
  State<BlocksManagerDisplay> createState() => _BlocksManagerState();
}

class _BlocksManagerState extends State<BlocksManagerDisplay> {
  String currentItem = "None";
  List<String> items = ["Please wait."];
  
  @override
  void initState() {
    super.initState();

    _init();
  }

  Future<void> _init() async {
    final channels = await invokeJS("get_channels");
    final soundboard = await invokeJS("get_soundboard");
    List<String> parsed = [];

    for (final channel in channels) {
      parsed.add(channel.name);
    }
    for (final sound in soundboard) {
      parsed.add(sound.name);
    }

    setState(() {items = parsed;});
  }
  
  @override
  Widget build(BuildContext context) {
    return AnimatedBuilder(
      animation: BlocksPageClass,
      builder: (context, _) {
        return Column(
          children: [
            Padding(
              padding: EdgeInsets.only(top: 12, left: 8, bottom: 4, right: 8),
              child: Row(
                children: [
                  Text("  Item:   ", style: TextStyle(color: text_muted, fontSize: 30)),
                  TextButton(
                    onPressed: () {
                      showDialog(
                        context: context,
                        builder: (context) => ItemsDropdown(
                          items: items,
                          onItemSelected: (optionSelected) {
                            setState(() {currentItem = optionSelected;});
                            BlocksPageClass.setPage(BlocksView(key: ValueKey(currentItem), item: currentItem, controller: blocksViewController));
                          },
                        ),
                      );
                    },
                    child: Text(currentItem, style: TextStyle(fontSize: 30, color: text))
                  ),

                  Spacer(),

                  ElevatedButton.icon(
                    style: TextButton.styleFrom(
                      backgroundColor: bg_light,
                      foregroundColor: accent
                    ),
                    onPressed: () => blocksViewController.save!(),
                    icon: const Icon(Icons.save),
                    label: Text("Save"),
                  ),
                ],
              ),
            ),

            Expanded(
              child: BlocksPageClass.page as Widget
            )
          ],
        );
      }
    );
  }
}

class BlocksPageHolder extends ChangeNotifier {
  Object? page = Container();

  void setPage(Object? newPage) {
    if (newPage is BlocksView || newPage is Container) {
      page = newPage;
      notifyListeners();
    }
  }
}

BlocksPageHolder BlocksPageClass = BlocksPageHolder();
final blocksViewController = BlocksViewController();
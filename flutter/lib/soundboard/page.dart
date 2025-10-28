import 'package:flutter/material.dart';
import '../settings/page.dart';
import 'new_page.dart';
import 'edit_page.dart';
import 'main_page.dart';

class SoundboardManagerDisplay extends StatefulWidget {
  const SoundboardManagerDisplay({super.key});

  @override
  State<SoundboardManagerDisplay> createState() => _SoundboardManagerState();
}

class _SoundboardManagerState extends State<SoundboardManagerDisplay> {
  @override
  Widget build(BuildContext context) {
    return AnimatedBuilder(
      animation: SoundboardPageClass,
      builder: (context, _) {
        return Column(
          children: [
            Padding(
              padding: EdgeInsets.only(top: 12, left: 8, bottom: 12),
              child: Row(
                children: [
                  ElevatedButton.icon(
                    style: TextButton.styleFrom(
                      backgroundColor: settings.lightMode ? const Color(0xFF262626) : Color(0xFFCCCCCC),
                      foregroundColor: Colors.purpleAccent
                    ),
                    onPressed: () => {SoundboardPageClass.setPage(SoundboardNew())},
                    icon: const Icon(Icons.add),
                    label: Text("Add"),
                  ),
                ],
              ),
            ),

            Expanded(
              child: SoundboardPageClass.page as Widget
            )
          ],
        );
      }
    );
  }
}

class SoundboardPageHolder extends ChangeNotifier {
  Object? page = SoundboardMain();

  void setPage(Object? newPage) {
    if (newPage is SoundboardMain || newPage is SoundboardEdit || newPage is SoundboardNew) {
      page = newPage;
      notifyListeners();
    }
  }
}

SoundboardPageHolder SoundboardPageClass = SoundboardPageHolder();
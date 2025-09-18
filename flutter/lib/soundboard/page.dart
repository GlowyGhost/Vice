import 'package:flutter/material.dart';
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
                    onPressed: () => {print("Add pressed for channels")},
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
    if (newPage is SoundboardMain || newPage is SoundboardEdit) {
      page = newPage;
      notifyListeners();
    }
  }
}

SoundboardPageHolder SoundboardPageClass = SoundboardPageHolder();
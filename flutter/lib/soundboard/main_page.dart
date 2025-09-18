import 'package:flutter/material.dart';
import 'package:vice/soundboard/edit_page.dart';
import 'package:vice/soundboard/page.dart';

class SoundboardMain extends StatefulWidget {
  const SoundboardMain({super.key});

  @override
  State<SoundboardMain> createState() => _SoundboardMainState();
}

class _SoundboardMainState extends State<SoundboardMain> {
  @override
  Widget build(BuildContext context) {
    final ScrollController scrollController = ScrollController();

    return Scaffold(
      body: Scrollbar(
        controller: scrollController,
        thumbVisibility: true,
        thickness: 12,
        radius: Radius.circular(32),
        trackVisibility: false,
        interactive: true,
        child: GridView(
          controller: scrollController,
          scrollDirection: Axis.vertical,
          gridDelegate: SliverGridDelegateWithFixedCrossAxisCount(
            crossAxisCount: (MediaQuery.of(context).size.width / 250).floor(),
            mainAxisSpacing: 8,
            crossAxisSpacing: 8,
            childAspectRatio: 1,
          ),
          children: [
            //TODO: Use user's actual Soundboard when backend is set up.
            createSoundboardButton(
              "Hello",
              Icons.waving_hand_rounded,
              Colors.blue
            ),
            createSoundboardButton(
              "Sun",
              Icons.light_mode,
              Colors.red
            ),
            createSoundboardButton(
              "Idea",
              Icons.lightbulb,
              Colors.green
            ),
            createSoundboardButton(
              "ABC",
              Icons.abc,
              Colors.orange
            ),
          ],
        ),
      ),
    );
  }

  Widget createSoundboardButton(String text, IconData icon, Color color) {
    return ElevatedButton(
      style: ElevatedButton.styleFrom(
        padding: EdgeInsets.all(16),
        backgroundColor: color,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(24),
        ),
      ),
      onPressed: () {
        print("Button clicked");
        // TODO: Tell Rust to play SFX
      },
      
      child: Stack(
        children: [
          Column(
            mainAxisAlignment: MainAxisAlignment.spaceBetween,
            children: [
              Expanded(
                flex: 2,
                child: FittedBox(
                  fit: BoxFit.contain,
                  child: Icon(icon, size: 128, color: Colors.white),
                ),
              ),
              Expanded(
                flex: 1,
                child: Center(
                  child: Text(text, style: TextStyle(fontSize: 48, fontWeight: FontWeight.bold, color: Colors.white)),
                ),
              ),
            ],
          ),
          Align(
            alignment: Alignment.bottomRight,
            child: IconButton(
              icon: Icon(Icons.more_vert, color: Colors.white, size: 32),
              onPressed: () => {SoundboardPageClass.setPage(SoundboardEdit(name: text))},
            ),
          ),
        ],
      ),
    );
  }
}
import 'package:flutter/material.dart';

class Soundboard extends StatefulWidget {
  const Soundboard({super.key});

  @override
  State<Soundboard> createState() => _SoundboardState();
}

class _SoundboardState extends State<Soundboard> {
  @override
  Widget build(BuildContext context) {
    final ScrollController scrollController = ScrollController();

    return Scaffold(
      body: Scrollbar(
        controller: scrollController,
        thumbVisibility: true, // always show scrollbar
        thickness: 12,         // custom thickness
        radius: Radius.circular(32), // rounded edges
        trackVisibility: false, // show track behind thumb
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
      //Icon(icon, size: 128, color: Colors.white)
      //Text(text, style: TextStyle(fontSize: 48, color: Colors.white))

      child: Stack(
        children: [
          // Main Column: icon at top, text at bottom
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
                  child: Text(text, style: TextStyle(fontSize: 48, color: Colors.white)),
                ),
              ),
            ],
          ),
          Positioned(
            bottom: 8,
            right: 8,
            child: PopupMenuButton<String>(
              icon: Icon(Icons.more_vert, color: Colors.white, size: 32),
              onSelected: (value) {
                if (value == "Option 1") {
                  print("Option 1 is selected on $text");
                } else if (value == "Option 2") {
                  print("Option 2 is selected on $text");
                }
              },
              itemBuilder: (context) => [
                PopupMenuItem(
                  value: "Option 1",
                  child: Text("Option 1")
                ),
                PopupMenuItem(
                  value: "Option 2",
                  child: Text("Option 2")
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
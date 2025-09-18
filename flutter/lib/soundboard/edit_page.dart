import 'package:flutter/material.dart';
import 'main_page.dart';
import 'page.dart';

class SoundboardEdit extends StatefulWidget {
  final String name;

  const SoundboardEdit({
    super.key,
    required this.name
  });

  @override
  State<SoundboardEdit> createState() => _SoundboardEditState();
}

class _SoundboardEditState extends State<SoundboardEdit> {
  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: Column(
        children: [
          Text(
            widget.name,
            style: const TextStyle(
                fontSize: 96,
                color: Colors.white
            )
          ),

          IconButton(
            onPressed: () => {SoundboardPageClass.setPage(SoundboardMain())},
            icon: const Icon(Icons.undo, size: 96, color: Colors.white,)
          )
        ],
      )
    );
  }
}
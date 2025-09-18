import 'package:flutter/material.dart';
import 'main_page.dart';
import 'page.dart';

class ChannelsEdit extends StatefulWidget {
  final String name;

  const ChannelsEdit({
    super.key,
    required this.name
  });

  @override
  State<ChannelsEdit> createState() => _ChannelsEditState();
}

class _ChannelsEditState extends State<ChannelsEdit> {
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
            onPressed: () => {ChannelsPageClass.setPage(ChannelsMain())},
            icon: const Icon(Icons.undo, size: 96, color: Colors.white,)
          )
        ],
      )
    );
  }
}
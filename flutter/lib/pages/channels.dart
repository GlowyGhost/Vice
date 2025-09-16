import 'package:flutter/material.dart';

class ChannelsPage extends StatefulWidget {
  const ChannelsPage({super.key});

  @override
  State<ChannelsPage> createState() => _ChannelsPageState();
}

class _ChannelsPageState extends State<ChannelsPage> {
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
          scrollDirection: Axis.horizontal,
          gridDelegate: SliverGridDelegateWithFixedCrossAxisCount(
            crossAxisCount: 1,
            mainAxisSpacing: 8,
            crossAxisSpacing: 8,
            childAspectRatio: 1/0.5,
          ),
          children: [
            //TODO: Use user's actual Channels when backend is set up.
            createChannelObject(
              "Hello",
              Icons.waving_hand_rounded,
              Colors.blue
            ),
            createChannelObject(
              "Sun",
              Icons.light_mode,
              Colors.red
            ),
            createChannelObject(
              "Idea",
              Icons.lightbulb,
              Colors.green
            ),
            createChannelObject(
              "ABC",
              Icons.abc,
              Colors.orange
            ),
          ],
        ),
      ),
    );
  }

  Widget createChannelObject(String text, IconData icon, Color defaultColor) {
    double volume = 0.5;

    return StatefulBuilder(
      builder: (context, setState) {
        Color getColor(double v) {
          if (v <= 0.7) return Colors.green;
          if (v <= 0.9) return Colors.orange;
          return Colors.red;
        }

        return Container(
          margin: const EdgeInsets.all(8),
          decoration: BoxDecoration(
            color: defaultColor,
            borderRadius: BorderRadius.circular(16),
          ),
          child: Stack(
            alignment: Alignment.center,
            children: [
              LayoutBuilder(
								builder: (context, constraints) {
									final sliderHeight = constraints.maxHeight - 150;

									return Stack(
										alignment: Alignment.center,
										children: [
											RotatedBox(
												quarterTurns: -1,
												child: Container(
													height: 10,
													width: sliderHeight - 50,
													decoration: BoxDecoration(
														color: Color.fromARGB(255, 60, 60, 60),
														borderRadius: BorderRadius.circular(12),
													),
												),
											),

											Align(
												alignment: Alignment.bottomCenter,
												child: Container(
													height: (sliderHeight - 40) * volume,
													width: 12,
													margin: EdgeInsets.only(
														bottom: (constraints.maxHeight - sliderHeight) / 2 + 25,
													),
													decoration: BoxDecoration(
														color: getColor(volume),
														borderRadius: BorderRadius.circular(12),
													),
												),
											),

											RotatedBox(
												quarterTurns: -1,
												child: SizedBox(
													width: sliderHeight,
													child: Slider(
														value: volume,
														thumbColor: Colors.white,
														inactiveColor: Colors.transparent,
														activeColor: Colors.transparent,
														onChanged: (val) {
															setState(() => volume = val);
														},
													),
												),
											),
										],
									);
								},
							),

							Align(
								alignment: Alignment.topCenter,
								child: CircleAvatar(
									radius: 50,
									backgroundColor: Colors.transparent,
                  child: Icon(icon, color: Colors.white, size: 50),
                ),
							),

							Align(
								alignment: Alignment.bottomCenter,
								child: Padding(
									padding: EdgeInsets.all(6),
									child: Text(
										text,
										style: const TextStyle(
											color: Colors.white,
											fontWeight: FontWeight.bold,
											fontSize: 48,
										),
									),
								)
							),

							Align(
								alignment: Alignment.bottomRight,
								child: Padding(
									padding: EdgeInsets.all(12),
									child: Icon(Icons.more_vert, size: 32, color: Colors.white)
								),
							),
            ],
          ),
        );
      },
    );
  }
}
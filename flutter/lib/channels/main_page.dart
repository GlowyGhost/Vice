import 'package:flutter/material.dart';
import '../invoke_js.dart';
import 'edit_page.dart';
import 'page.dart';
import '../icons.dart';

class ChannelsColor {
	int r;
	int g;
	int b;

	ChannelsColor(this.r, this.g, this.b);
}

class ChannelsClass {
  String name;
  String icon;
	ChannelsColor color;
  String sound;

  ChannelsClass(this.name, this.icon, this.color, this.sound);
}

class ChannelsMain extends StatefulWidget {
  const ChannelsMain({super.key});

  @override
  State<ChannelsMain> createState() => _ChannelsMainState();
}

class _ChannelsMainState extends State<ChannelsMain> {
  bool _loading = true;
  List<ChannelsClass>? Channels;

  @override
  void initState() {
    super.initState();

    _init();
  }

  Future<void> _init() async {
    if (mounted && isTauriAvailable == false) {
      printText("Can't connect to Backend.");
      snackBar("Unable to connect with backend.");
      return;
    }
    
    _loadChannels();
  }

  Future<void> _loadChannels() async {
    setState(() => _loading = true);

    final channels = await invokeJS("get_channels");

    List<ChannelsClass> covertedChannels = [];

    for (final SFXsChannels channel in channels) {
			covertedChannels.add(ChannelsClass(channel.name!, channel.icon!, ChannelsColor(channel.color![0], channel.color![1], channel.color![2]), channel.device!));
    }
    
    setState(() {
      Channels = covertedChannels;
      _loading = false;
    });
  }

  void snackBar(String text) {
    ScaffoldMessenger.of(context).showSnackBar(
			SnackBar(content: Text(text)),
		);
  }

  @override
  Widget build(BuildContext context) {
    final ScrollController scrollController = ScrollController();

    return Scaffold(
      body: _loading
        ? const Center(child: CircularProgressIndicator())
        : Channels!.isEmpty
          ? const Center(child: Text("No Channels Found."))
          : Scaffold(
              body: Scrollbar(
                controller: scrollController,
                thumbVisibility: true,
                thickness: 12,
                radius: Radius.circular(32),
                trackVisibility: false,
                interactive: true,
                child: Padding(
                  padding: EdgeInsets.only(left: 8, bottom: 8),
                  child: GridView.builder(
                    controller: scrollController,
                    scrollDirection: Axis.horizontal,
                    itemCount: Channels?.length,
                    gridDelegate: SliverGridDelegateWithFixedCrossAxisCount(
                      crossAxisCount: 1,
                      mainAxisSpacing: 8,
                      crossAxisSpacing: 8,
                      childAspectRatio: 1/0.5,
                    ),
                    itemBuilder: (context, index) {
                      ChannelsClass? channel = Channels?[index];

                      String? name = channel?.name;
                      IconData icon = getIcon(channel!.icon);
                      Color color = Color.fromARGB(255, channel.color.r, channel.color.g, channel.color.b);

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
                              color: color,
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
                                      name!,
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
                                    child: IconButton(
                                      onPressed: () => {ChannelsPageClass.setPage(ChannelsEdit(name: name))},
                                      icon: Icon(Icons.more_vert, size: 32, color: Colors.white)
                                    )
                                  ),
                                ),
                              ],
                            ),
                          );
                        },
                      );
                    },
                  ),
                )
              ),
            )
    );
  }
}
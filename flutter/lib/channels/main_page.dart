import 'package:flutter/material.dart';
import 'package:vice/settings/page.dart';
import 'dart:async';
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
  String device;
  bool deviceOrApp;
  bool lowlatency;
  double volume;

  ChannelsClass(this.name, this.icon, this.color, this.device, this.deviceOrApp, this.lowlatency, this.volume);
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

    List<ChannelsClass> convertedChannels = [];

    for (final SFXsChannels channel in channels) {
      convertedChannels.add(ChannelsClass(channel.name!, channel.icon!, ChannelsColor(channel.color![0], channel.color![1], channel.color![2]), channel.device!, channel.deviceOrApp!, channel.lowlatency!, channel.volume!));
    }
    
    setState(() {
      Channels = convertedChannels;
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
                      return ChannelBar(channel: Channels![index]);
                    },
                  ),
                )
              ),
            )
    );
  }
}

class ChannelBar extends StatefulWidget {
  final ChannelsClass channel;

  const ChannelBar({Key? key, required this.channel}) : super(key: key);

  @override
  State<ChannelBar> createState() => _ChannelBarState();
}

class _ChannelBarState extends State<ChannelBar> {
  double sliderVolume = 0;
  late double volume;
  String translatedName = "null";
  Timer? _timer;

  @override
  void initState() {
    super.initState();
    sliderVolume = widget.channel.volume / 2;

    _initVolume();
  }

  @override
  void dispose() {
    if (_timer != null) _timer?.cancel();
    super.dispose();
  }

  Future<void> _initVolume() async {
    if (settings.peaks == true) {
      await _getVolume();
      _timer = Timer.periodic(const Duration(milliseconds: 100), (_) => _getVolume());
    }
  }

  Future<void> _getVolume() async {
    String result = await invokeJS("get_volume", {
      "name": translatedName == "null" ? widget.channel.device : translatedName,
      "get": translatedName == "null",
      "device": widget.channel.deviceOrApp
    });

    setState(() {volume = double.parse(result.split("¬")[0]) * sliderVolume;});

    if (translatedName == "null")
      setState(() {translatedName = result.split("¬")[1];});

  }

  Future<void> _setVolume() async {
    await invokeJS("set_volume", {"name": widget.channel.name, "volume": sliderVolume * 2});
  }

  Color getColor(double v) {
    if (v <= 0.4) return Colors.green;
    if (v <= 0.5) return Colors.orangeAccent;
    if (v <= 0.6) return Colors.orange;
    return Colors.red;
  }

  @override
  Widget build(BuildContext context) {
    return Container(
      margin: const EdgeInsets.all(8),
      decoration: BoxDecoration(
        color: Color.fromARGB(255, widget.channel.color.r, widget.channel.color.g, widget.channel.color.b),
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
                  child: AnimatedContainer(
                    duration: const Duration(milliseconds: 200),
                    height: (sliderHeight - 40) * volume,
                    width: 12,
                    margin: EdgeInsets.only(bottom: (constraints.maxHeight - sliderHeight) / 2 + 25),
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
                        value: sliderVolume,
                        thumbColor: Colors.white,
                        inactiveColor: Colors.transparent,
                        activeColor: Colors.transparent,
                        onChanged: (val) {
                          double v = val;
                          if (v < 0.55 && v > 0.45) {
                            v = 0.5;
                          }
                          setState(() => sliderVolume = v);
                          _setVolume();
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
              child: Icon(getIcon(widget.channel.icon), color: Colors.white, size: 50),
            ),
          ),

          Align(
            alignment: Alignment.bottomCenter,
            child: Padding(
              padding: EdgeInsets.all(6),
              child: Text(
                widget.channel.name,
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
                onPressed: () => {ChannelsPageClass.setPage(ChannelsEdit(name: widget.channel.name, icon: widget.channel.icon, color: [widget.channel.color.r, widget.channel.color.g, widget.channel.color.b], deviceApp: widget.channel.device, deviceBool: widget.channel.deviceOrApp, lowlatency: widget.channel.lowlatency))},
                icon: Icon(Icons.more_vert, size: 32, color: Colors.white)
              )
            ),
          ),
        ],
      ),
    );
  }
}
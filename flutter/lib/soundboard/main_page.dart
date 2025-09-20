import 'package:flutter/material.dart';
import '../invoke_js.dart';
import 'edit_page.dart';
import 'page.dart';
import '../util.dart';

class SFXColor {
	int r;
	int g;
	int b;

	SFXColor(this.r, this.b, this.g);
}

class SFXClass {
  String name;
  String icon;
	SFXColor color;
  String sound;

  SFXClass(this.name, this.icon, this.color, this.sound);
}

class SoundboardMain extends StatefulWidget {
  const SoundboardMain({super.key});

  @override
  State<SoundboardMain> createState() => _SoundboardMainState();
}

class _SoundboardMainState extends State<SoundboardMain> {
  bool _loading = true;
  List<SFXClass>? SFXs;

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

    _loadSfx();
  }

  Future<void> _loadSfx() async {
    setState(() => _loading = true);

    final sfxs = await invokeJS("get_soundboard");

    List<SFXClass> covertedSFXS = [];

    for (final SFXsChannels sfx in sfxs) {
			covertedSFXS.add(SFXClass(sfx.name!, sfx.icon!, SFXColor(sfx.color![0], sfx.color![1], sfx.color![2]), sfx.sound!));
    }
    
    setState(() {
      SFXs = covertedSFXS;
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
        : SFXs!.isEmpty
          ? const Center(child: Text("No Sound Effects Found."))
          : Scaffold(
              body: Scrollbar(
                controller: scrollController,
                thumbVisibility: true,
                thickness: 12,
                radius: Radius.circular(32),
                trackVisibility: false,
                interactive: true,
								child: Padding(
									padding: EdgeInsets.only(left: 8, right: 8),
									child: GridView.builder(
										controller: scrollController,
										scrollDirection: Axis.vertical,
										itemCount: SFXs?.length,
										gridDelegate: SliverGridDelegateWithFixedCrossAxisCount(
											crossAxisCount: (MediaQuery.of(context).size.width / 250).floor(),
											mainAxisSpacing: 8,
											crossAxisSpacing: 8,
											childAspectRatio: 1,
										),
										itemBuilder: (context, index) {
											SFXClass? sfx = SFXs?[index];

											String? name = sfx?.name;
											IconData icon = getIcon(sfx!.icon);
											Color color = Color.fromARGB(255, sfx.color.r, sfx.color.g, sfx.color.b);

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
																		child: Text(name!, style: TextStyle(fontSize: 48, fontWeight: FontWeight.bold, color: Colors.white)),
																	),
																),
															],
														),
														Align(
															alignment: Alignment.bottomRight,
															child: IconButton(
																icon: Icon(Icons.more_vert, color: Colors.white, size: 32),
																onPressed: () => {SoundboardPageClass.setPage(SoundboardEdit(name: name))},
															),
														),
													],
												),
											);
										}
									),
								),
              ),
            )
    );
  }
}
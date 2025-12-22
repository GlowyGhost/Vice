import 'dart:convert';
import 'package:flutter/material.dart';
import '../invoke_js.dart';
import '../randoms.dart';
import 'types/compression.dart';
import 'types/delay.dart';
import 'types/distortion.dart';
import 'types/gain.dart';
import 'types/gating.dart';
import 'types/reverb.dart';

final Map<String, Widget> blockTypes = {
  "compression": CompressionBlock(interactable: false),
  "delay": DelayBlock(interactable: false),
  "distortion": DistortionBlock(interactable: false),
  "gating": GatingBlock(interactable: false),
  "reverb": ReverbBlock(interactable: false),
  "gain": GainBlock(interactable: false),
};

class BlocksView extends StatefulWidget {
  final String item;
  final BlocksViewController controller;
  const BlocksView({super.key, required this.item, required this.controller});

  @override
  State<BlocksView> createState() => _BlocksViewState();
}

class _BlocksViewState extends State<BlocksView> {
  late final ScrollController scrollController;
  List<Map<String, dynamic>> Blocks = [];
  bool _loading = true;

  @override
  void initState() {
    super.initState();

    widget.controller.save = save;
    scrollController = ScrollController();

    _init();
  }

  void _init() async {
    final result = await invokeJS("load_blocks", {"item": widget.item});
    setState(() {
      Blocks = List<Map<String, dynamic>>.from(jsonDecode(result));
      _loading = false;
    });
  }

  void save() {
    final json = jsonEncode(Blocks);
    invokeJS("save_blocks", {"item": widget.item, "blocks": json});
  }

  @override
  void dispose() {
    scrollController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    int container_width = (MediaQuery.of(context).size.width / 1.5).toInt();

    return Padding(
      padding: EdgeInsets.all(16),
      child: Row(
        children: [
          Container(
            width: container_width.toDouble(),
            decoration: BoxDecoration(
              borderRadius: BorderRadius.circular(8),
              color: bg_light,
            ),
            child: _loading
              ? Center(child: CircularProgressIndicator())
              : Blocks.isEmpty
                ? Center(child: Text("No blocks added.", style: TextStyle(color: text, fontSize: 16)))
                : Scrollbar(
                  controller: scrollController,
                  thumbVisibility: true,
                  thickness: 12,
                  radius: const Radius.circular(12),
                  trackVisibility: false,
                  interactive: true,
                  child: ListView.builder(
                    controller: scrollController,
                    padding: const EdgeInsets.all(8),
                    itemCount: Blocks.length,
                    itemBuilder: (context, index) {
                      final block = Blocks[index];
                      final type = block["type"];
                      switch (type) {
                        case "delay":
                          return Padding(
                            padding: EdgeInsetsGeometry.all(8),
                            child: DelayBlock(
                              time: block["time"],
                              interactable: true,
                              onChanged: (newTime) {
                                setState(() {
                                  block["time"] = newTime;
                                });
                              },
                              onDelete: () => {
                                setState(() {
                                  Blocks.removeAt(index);
                                })
                              },
                            )
                          );
                        case "reverb":
                          return Padding(
                            padding: EdgeInsetsGeometry.all(8),
                            child: ReverbBlock(
                              intensity: block["intensity"],
                              interactable: true,
                              onChanged: (newIntensity) {
                                setState(() {
                                  block["intensity"] = newIntensity;
                                });
                              },
                              onDelete: () => {
                                setState(() {
                                  Blocks.removeAt(index);
                                })
                              },
                            )
                          );
                        case "compression":
                          return Padding(
                            padding: EdgeInsetsGeometry.all(8),
                            child: CompressionBlock(
                              amount: block["amount"],
                              interactable: true,
                              onChanged: (newAmount) {
                                setState(() {
                                  block["amount"] = newAmount;
                                });
                              },
                              onDelete: () => {
                                setState(() {
                                  Blocks.removeAt(index);
                                })
                              },
                            )
                          );
                        case "distortion":
                          return Padding(
                            padding: EdgeInsetsGeometry.all(8),
                            child: DistortionBlock(
                              intensity: block["intensity"],
                              interactable: true,
                              onChanged: (newIntensity) {
                                setState(() {
                                  block["intensity"] = newIntensity;
                                });
                              },
                              onDelete: () => {
                                setState(() {
                                  Blocks.removeAt(index);
                                })
                              },
                            )
                          );
                        case "gain":
                          return Padding(
                            padding: EdgeInsetsGeometry.all(8),
                            child: GainBlock(
                              amount: block["amount"],
                              interactable: true,
                              onChanged: (newAmount) {
                                setState(() {
                                  block["amount"] = newAmount;
                                });
                              },
                              onDelete: () => {
                                setState(() {
                                  Blocks.removeAt(index);
                                })
                              },
                            )
                          );
                        case "gating":
                          return Padding(
                            padding: EdgeInsetsGeometry.all(8),
                            child: GatingBlock(
                              threshold: block["threshold"],
                              interactable: true,
                              onChanged: (newThreshold) {
                                setState(() {
                                  block["threshold"] = newThreshold;
                                });
                              },
                              onDelete: () => {
                                setState(() {
                                  Blocks.removeAt(index);
                                })
                              },
                            )
                          );
                        default:
                          return Container();
                      }
                    },
                  )
                )
              ),

          SizedBox(width: 20),

          Expanded(
            child: Container(
              decoration: BoxDecoration(
                borderRadius: BorderRadius.circular(8),
                color: bg_light,
              ),
              child: Scrollbar(
                controller: scrollController,
                thumbVisibility: true,
                thickness: 12,
                radius: const Radius.circular(12),
                trackVisibility: false,
                interactive: true,
                child: ListView.builder(
                  controller: scrollController,
                  padding: const EdgeInsets.all(8),
                  itemCount: blockTypes.length,
                  itemBuilder: (context, index) {
                    final blockType = blockTypes.values.toList()[index];
                    return Padding(
                      padding: const EdgeInsets.only(bottom: 8),
                      child: GestureDetector(
                        onTap: () {
                          switch (blockTypes.keys.toList()[index]) {
                            case "delay":
                              setState(() {
                                Blocks.add({"type": "delay", "time": 1000.0});
                              });
                              break;
                            case "reverb":
                              setState(() {
                                Blocks.add({"type": "reverb", "intensity": 50.0});
                              });
                              break;
                            case "compression":
                              setState(() {
                                Blocks.add({"type": "compression", "amount": 50.0});
                              });
                              break;
                            case "distortion":
                              setState(() {
                                Blocks.add({"type": "distortion", "intensity": 50.0});
                              });
                              break;
                            case "gain":
                              setState(() {
                                Blocks.add({"type": "gain", "amount": 1});
                              });
                              break;
                            case "gating":
                              setState(() {
                                Blocks.add({"type": "gating", "threshold": 50.0});
                              });
                              break;
                            default:
                              break;
                          }
                        },
                        child: Padding(
                          padding: EdgeInsets.all(8),
                          child: SizedBox(
                            height: 100,
                            child: blockType,
                          ),
                        ),
                      ),
                    );
                  },
                )
              )
            )
          ),
        ],
      )
    );
  }
}

class BlocksViewController {
  void Function()? save;
}
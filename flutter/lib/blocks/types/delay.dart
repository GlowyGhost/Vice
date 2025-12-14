import 'package:flutter/material.dart';
import '../../randoms.dart';

class DelayBlock extends StatelessWidget {
  final double? time;
  final bool interactable;
  final ValueChanged<double>? onChanged;

  const DelayBlock({
    super.key,
    this.time,
    this.interactable = true,
    this.onChanged,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        color: Colors.blue,
        borderRadius: BorderRadius.circular(8),
      ),
      height: 100,
      child: Row(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          const Text(
            "DELAY",
            style: TextStyle(
              color: Colors.white,
              fontWeight: FontWeight.bold,
              fontSize: 48,
            ),
          ),
          const SizedBox(width: 16),
          Expanded(
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Slider(
                  value: time ?? 1000,
                  min: 0,
                  max: 2000,
                  divisions: 20,
                  activeColor: accent,
                  label: time?.toStringAsFixed(1),
                  onChanged: interactable ? onChanged : null,
                ),
                Text(
                  "Time: ${time?.toStringAsFixed(1) ?? '1000'}ms",
                  style: const TextStyle(color: Colors.white, fontSize: 16),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
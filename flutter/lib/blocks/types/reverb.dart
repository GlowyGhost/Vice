import 'package:flutter/material.dart';
import '../../randoms.dart';

class ReverbBlock extends StatelessWidget {
  final double? intensity;
  final bool interactable;
  final ValueChanged<double>? onChanged;

  const ReverbBlock({
    super.key,
    this.intensity,
    this.interactable = true,
    this.onChanged,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        color: Colors.deepOrange,
        borderRadius: BorderRadius.circular(8),
      ),
      height: 100,
      child: Row(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          const Text(
            "REVERB",
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
                  value: intensity ?? 50,
                  min: 0,
                  max: 100,
                  divisions: 100,
                  activeColor: accent,
                  label: intensity?.toStringAsFixed(1),
                  onChanged: interactable ? onChanged : null,
                ),
                Text(
                  "intensity: ${intensity?.toStringAsFixed(1) ?? '50'}%",
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
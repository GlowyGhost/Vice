import 'package:flutter/material.dart';
import '../../randoms.dart';

class CompressionBlock extends StatelessWidget {
  final double? amount;
  final bool interactable;
  final ValueChanged<double>? onChanged;

  const CompressionBlock({
    super.key,
    this.amount,
    this.interactable = true,
    this.onChanged,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        color: Colors.teal,
        borderRadius: BorderRadius.circular(8),
      ),
      height: 100,
      child: Row(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          const Text(
            "COMPRESSION",
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
                  value: amount ?? 50,
                  min: 0,
                  max: 100,
                  divisions: 100,
                  activeColor: accent,
                  label: amount?.toStringAsFixed(1),
                  onChanged: interactable ? onChanged : null,
                ),
                Text(
                  "Amount: ${amount?.toStringAsFixed(1) ?? '50'}%",
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
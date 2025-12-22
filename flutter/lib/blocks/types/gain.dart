import 'package:flutter/material.dart';
import '../../randoms.dart';

class GainBlock extends StatelessWidget {
  final double? amount;
  final bool interactable;
  final ValueChanged<double>? onChanged;
  final Function? onDelete;

  const GainBlock({
    super.key,
    this.amount,
    this.interactable = true,
    this.onChanged,
    this.onDelete
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        color: Colors.pinkAccent,
        borderRadius: BorderRadius.circular(8),
      ),
      height: 100,
      child: Stack(
        children: [
          Row(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              const Text(
                "GAIN",
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
                      value: amount ?? 1,
                      min: 0,
                      max: 3,
                      divisions: 30,
                      activeColor: accent,
                      label: amount?.toStringAsFixed(1),
                      onChanged: interactable ? onChanged : null,
                    ),
                    Text(
                      "Time: ${amount?.toStringAsFixed(1) ?? '1'}x",
                      style: const TextStyle(color: Colors.white, fontSize: 16),
                    ),
                  ],
                ),
              ),
            ],
          ),
          Positioned(
            bottom: 0,
            right: 0,
            child: IconButton(
              icon: Icon(Icons.delete, color: Colors.white),
              onPressed: onDelete != null ? () => onDelete!() : null,
            ),
          ),
        ],
      ),
    );
  }
}
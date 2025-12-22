import 'package:flutter/material.dart';
import '../../randoms.dart';

class DelayBlock extends StatelessWidget {
  final double? time;
  final bool interactable;
  final ValueChanged<double>? onChanged;
  final Function? onDelete;

  const DelayBlock({
    super.key,
    this.time,
    this.interactable = true,
    this.onChanged,
    this.onDelete,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        color: Colors.blue,
        borderRadius: BorderRadius.circular(8),
      ),
      height: 100,
      child: Stack(
        children: [
          Row(
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
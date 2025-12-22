import 'package:flutter/material.dart';
import '../../randoms.dart';

class GatingBlock extends StatelessWidget {
  final double? threshold;
  final bool interactable;
  final ValueChanged<double>? onChanged;
  final Function? onDelete;

  const GatingBlock({
    super.key,
    this.threshold,
    this.interactable = true,
    this.onChanged,
    this.onDelete,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        color: Colors.purple,
        borderRadius: BorderRadius.circular(8),
      ),
      height: 100,
      child: Stack(
        children: [
          Row(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              const Text(
                "GATING",
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
                      value: threshold ?? 50,
                      min: 0,
                      max: 100,
                      divisions: 100,
                      activeColor: accent,
                      label: threshold?.toStringAsFixed(1),
                      onChanged: interactable ? onChanged : null,
                    ),
                    Text(
                      "Threshold: ${threshold?.toStringAsFixed(1) ?? '50'}%",
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
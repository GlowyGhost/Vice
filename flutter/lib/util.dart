import 'package:flutter/material.dart';

IconData getIcon(String icon) {
  switch (icon.toUpperCase()) {
    case "ABC":
      return Icons.abc;
    default:
      return Icons.question_mark;
  }
}
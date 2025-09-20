import 'package:flutter/material.dart';

Map<String, IconData> IconMap = {
  "abc": Icons.abc,
  "question_mark": Icons.question_mark,
};

IconData getIcon(String iconStr) {
  return IconMap[iconStr.toLowerCase()] ?? Icons.question_mark;
}

String fromIcon(IconData icon) {
  for (String iconStr in IconMap.keys) {
    if (IconMap[iconStr] == icon) {
      return iconStr;
    }
  }

  return "question_mark";
} 

class IconGridDropdown extends StatelessWidget {
  final List<IconData> icons;
  final ValueChanged<IconData> onIconSelected;

  IconGridDropdown({required this.icons, required this.onIconSelected});

  @override
  Widget build(BuildContext context) {
    return Dialog(
      backgroundColor: Color(0xFF363636),
      insetPadding: EdgeInsets.all(20),
      child: Container(
        width: double.maxFinite,
        height: 300,
        padding: EdgeInsets.all(16),
        child: GridView.builder(
          itemCount: icons.length,
          gridDelegate: SliverGridDelegateWithFixedCrossAxisCount(
            crossAxisCount: 10,
            mainAxisSpacing: 16,
            crossAxisSpacing: 16,
          ),
          itemBuilder: (context, index) {
            return InkWell(
              onTap: () {
                onIconSelected(icons[index]);
                Navigator.pop(context);
              },
              child: Container(
                decoration: BoxDecoration(
                  color: Color(0xFF3F3F3F),
                  borderRadius: BorderRadius.circular(8),
                ),
                child: Icon(icons[index], size: 30, color: Colors.white),
              ),
            );
          },
        ),
      ),
    );
  }
}
import 'package:flutter/material.dart';
import 'settings/page.dart';

Color get bg_dark => settings.lightMode ? Color(0xFFE6E6E6) : Color(0xFF1A1A1A);
Color get bg_mid => settings.lightMode ? Color(0xFFF2F2F2) : Color(0xFF262626);
Color get bg_light => settings.lightMode ? Color(0xFFFFFFFF) : Color(0xFF333333);
Color get text => settings.lightMode ? Color(0xFF000000) : Color(0xFFFFFFFF);
Color get text_muted => settings.lightMode ? Color(0xFF4F4F4F) : Color(0xFFB3B3B3);
final Color accent = Color(0XFFFFCE3B);

Map<String, IconData> IconMap = {
  "abc": Icons.abc,
  "ac_unit": Icons.ac_unit,
  "access_alarm": Icons.access_alarm,
  "access_time": Icons.access_time,
  "accessibility": Icons.accessibility,
  "account_balance": Icons.account_balance,
  "add": Icons.add,
  "adobe": Icons.adobe,
  "airline_seat_flat": Icons.airline_seat_flat,
  "android": Icons.android,
  "animation": Icons.animation,
  "apple": Icons.apple,
  "arrow_back": Icons.arrow_back,
  "assignment": Icons.assignment,
  "attachment": Icons.attachment,
  "backpack": Icons.backpack,
  "battery_4": Icons.battery_4_bar,
  "battery_alert": Icons.battery_alert,
  "beach_access": Icons.beach_access,
  "bloodtype": Icons.bloodtype,
  "bluetooth": Icons.bluetooth,
  "bookmark": Icons.bookmark,
  "bug_report": Icons.bug_report,
  "build": Icons.build,
  "cake": Icons.cake,
  "calculate": Icons.calculate,
  "cancel": Icons.cancel,
  "castle": Icons.castle,
  "check_box": Icons.check_box,
  "circle": Icons.circle,
  "cloud": Icons.cloud,
  "code": Icons.code,
  "computer": Icons.computer,
  "contactless": Icons.contactless,
  "create_new_folder": Icons.create_new_folder,
  "data_object": Icons.data_object,
  "delete_forever": Icons.delete_forever,
  "desktop_mac": Icons.desktop_mac,
  "desktop_windows": Icons.desktop_windows,
  "directions": Icons.directions,
  "do_disturb": Icons.do_disturb,
  "download": Icons.download,
  "duo": Icons.duo,
  "eject": Icons.eject,
  "email": Icons.email,
  "face": Icons.face,
  "filter": Icons.filter,
  "fingerprint": Icons.fingerprint,
  "flip": Icons.flip,
  "forklift": Icons.forklift,
  "forward_30": Icons.forward_30,
  "games": Icons.games,
  "grid_4x4": Icons.grid_4x4,
  "hail": Icons.hail,
  "help": Icons.help,
  "hotel": Icons.hotel,
  "image": Icons.image,
  "iso": Icons.iso,
  "keyboard": Icons.keyboard,
  "kitchen": Icons.kitchen,
  "lens": Icons.lens,
  "lock": Icons.lock,
  "money": Icons.money,
  "mouse": Icons.mouse,
  "nature": Icons.nature,
  "network_wifi_3_bar": Icons.network_wifi_3_bar,
  "note": Icons.note,
  "numbers": Icons.numbers,
  "pages": Icons.pages,
  "pattern": Icons.pattern,
  "percent": Icons.percent,
  "phone": Icons.phone,
  "photo": Icons.photo,
  "plagiarism": Icons.plagiarism,
  "power": Icons.power,
  "publish": Icons.publish,
  "question_mark": Icons.question_mark,
  "queue": Icons.queue,
  "reddit": Icons.reddit,
  "replay": Icons.replay,
  "rocket": Icons.rocket,
  "satellite": Icons.satellite,
  "science": Icons.science,
  "sell": Icons.sell,
  "settings": Icons.settings,
  "signal_cellular_0_bar": Icons.signal_cellular_0_bar,
  "sip": Icons.sip,
  "soap": Icons.soap,
  "sports": Icons.sports,
  "star": Icons.star,
  "style": Icons.style,
  "swipe": Icons.swipe,
  "tab": Icons.tab,
  "terminal": Icons.terminal,
  "thunderstorm": Icons.thunderstorm,
  "tiktok": Icons.tiktok,
  "train": Icons.train,
  "tv": Icons.tv,
  "update": Icons.update,
  "usb": Icons.usb,
  "video_call": Icons.video_call,
  "water": Icons.water,
  "wifi": Icons.wifi,
  "wysiwyg": Icons.wysiwyg,
  "zoom_in": Icons.zoom_in,
  "zoom_out": Icons.zoom_out
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

  const IconGridDropdown({super.key, required this.icons, required this.onIconSelected});

  @override
  Widget build(BuildContext context) {
    return Dialog(
      backgroundColor: bg_mid,
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
                  color: bg_light,
                  borderRadius: BorderRadius.circular(8),
                ),
                child: Icon(icons[index], size: 30, color: text),
              ),
            );
          },
        ),
      ),
    );
  }
}

class ItemsDropdown extends StatelessWidget {
  final List<String> items;
  final ValueChanged<String> onItemSelected;

  const ItemsDropdown({super.key, required this.items, required this.onItemSelected});

  @override
  Widget build(BuildContext context) {
    return Dialog(
      backgroundColor: bg_mid,
      insetPadding: EdgeInsets.all(20),
      child: Container(
        width: double.maxFinite,
        height: 300,
        padding: EdgeInsets.all(16),
        child: ListView.builder(
          itemCount: items.length,
          itemBuilder: (context, index) {
            return InkWell(
              onTap: () {
                onItemSelected(items[index]);
                Navigator.pop(context);
              },
              child: Padding(
                padding: EdgeInsets.all(8),
                child: Container(
                  decoration: BoxDecoration(
                    color: bg_light,
                    borderRadius: BorderRadius.circular(8),
                  ),
                  child: Text(items[index], style: TextStyle(fontSize: 30, color: Colors.white)),
                ),
              )
            );
          },
        ),
      ),
    );
  }
}
import "package:js/js.dart";
import "dart:js_util" as js_util;
import "dart:js" as js;
import "dart:async";

@JS("window.__TAURI__.core.invoke")
external dynamic _invoke(String cmd, [dynamic args]);

Future<dynamic> invokeJS(String cmd, [Map<String, dynamic>? args]) async {
  try {
    final jsArgs = args != null ? js_util.jsify(args) : null;
    final promise = _invoke(cmd, jsArgs);
    final result = await js_util.promiseToFuture(promise);

    if (result is String) {
      return result;
    } else if (result == null) {
      return null;
    } else if (result is List) {
      return result.map((e) {
        if (e is String) {
          return e;
        }

        final map = toMap(e);

        if (map.containsKey("name") ||
            map.containsKey("icon") ||
            map.containsKey("sound") ||
            map.containsKey("device") ||
            map.containsKey("color") ||
            map.containsKey("lowlatency")) {
          return SFXsChannels.fromMap(map);
        }

        return e.toString();
      }).toList();
    }

    return result;
  } catch (e) {
    printText("Error during JS invoking: $e");
    return;
  }
}

Map<String, dynamic> toMap(dynamic jsObject) {
  if (jsObject is Map<String, dynamic>) {
    return jsObject;
  } else if (jsObject != null) {
    final map = <String, dynamic>{};
    final keys = js_util.objectKeys(jsObject);
    for (var i = 0; i < keys.length; i++) {
      final key = keys[i]?.toString();
      if (key != null) {
        map[key] = js_util.getProperty(jsObject, key);
      }
    }
    return map;
  } else {
    return {};
  }
}

Future<void> printText(String? text) async {
  String trueText = "null";
  if (text != null) {
    trueText = text;
  }

  await invokeJS("flutter_print", {"text": trueText});
}

bool get isTauriAvailable {
	final tauri = js.context["__TAURI__"];
	return tauri != null && tauri is js.JsObject;
}

class SFXsChannels {
  final String? name;
  final String? icon;
  final String? sound;
  final String? device;
  final List<int>? color;
  final bool? deviceOrApp;
  final bool? lowlatency;

  SFXsChannels({
    this.name,
    this.icon,
    this.sound,
    this.device,
    this.color,
    this.deviceOrApp,
    this.lowlatency,
  });

  factory SFXsChannels.fromMap(Map<String, dynamic> map) {
    List<int>? parsedColor = [0, 0, 0];
    if (map["color"] is List) {
      parsedColor = List<int>.from(map["color"]);
    }

    return SFXsChannels(
      name: map["name"],
      icon: map["icon"],
      sound: map["sound"],
      device: map["device"],
      color: parsedColor,
      deviceOrApp: map["deviceorapp"],
      lowlatency: map["lowlatency"],
    );
  }
}
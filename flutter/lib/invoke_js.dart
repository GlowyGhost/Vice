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

    if (result is String || result is bool) {
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
    } else {
      return toMap(result);
    }
  } catch (e) {
    printText("Error during JS invoking: $e");
    return;
  }
}

Map<String, dynamic> toMap(dynamic jsObject) {
  if (jsObject == null) return {};

  if (jsObject is Map) {
    return Map<String, dynamic>.from(jsObject);
  }

  final map = <String, dynamic>{};
  try {
    final keys = js_util.objectKeys(jsObject);
    for (final key in keys) {
      final keyStr = key?.toString();
      if (keyStr != null) {
        final value = js_util.getProperty(jsObject, keyStr);
        map[keyStr] = value;
      }
    }
  } catch (e) {
    printText("Failed to convert JS object to map: $e");
  }
  return map;
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
  final double? volume;

  SFXsChannels({
    this.name,
    this.icon,
    this.sound,
    this.device,
    this.color,
    this.deviceOrApp,
    this.lowlatency,
    this.volume
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
      volume: map["volume"]
    );
  }
}

class Settings {
  final String? output;
  final double? scale;
  final bool? light;
  final bool? monitor;

  Settings({
    this.output,
    this.scale,
    this.light,
    this.monitor
  });

  factory Settings.fromMap(Map<String, dynamic> map) {
    return Settings(
      output: map["output"],
      scale: map["scale"],
      light: map["light"],
      monitor: map["monitor"]
    );
  }
}
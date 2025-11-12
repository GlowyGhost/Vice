import "dart:js_util";
import 'dart:html' as html;
import 'dart:convert';
import "dart:async";

Future<dynamic> invokeJS(String cmd, [Map<String, dynamic>? args]) async {
  final body = jsonEncode({'cmd': cmd, 'args': args ?? {}});
  try {
    final resp = await html.HttpRequest.request(
      '/ipc',
      method: 'POST',
      sendData: body,
      requestHeaders: {'Content-Type': 'application/json'},
    ).timeout(Duration(seconds: 10));

    if (resp.status == 200) {
      final text = resp.responseText ?? 'null';
      final decoded = jsonDecode(text);
      if (decoded == null) {
        return null;
      }
      final result = decoded['result'];
      
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
    } else {
      print('IPC HTTP ${resp.status}: ${resp.statusText}');
      return null;
    }
  } on TimeoutException catch (e) {
    print('IPC request timed out: $e');
    return null;
  } catch (e) {
    print('IPC request failed: $e');
    return null;
  }
}

Map<String, dynamic> toMap(dynamic jsObject) {
  if (jsObject == null) return {};

  if (jsObject is Map) {
    return Map<String, dynamic>.from(jsObject);
  }

  final map = <String, dynamic>{};
  try {
    final keys = objectKeys(jsObject);
    for (final key in keys) {
      final keyStr = key?.toString();
      if (keyStr != null) {
        final value = getProperty(jsObject, keyStr);
        map[keyStr] = value;
      }
    }
  } catch (e) {
    printText("Failed to convert JS object to map: $e");
  }
  return map;
}

bool _isPrintingText = false;

Future<void> printText(String? text) async {
  String trueText = "null";
  if (text != null) {
    trueText = text;
  }
  print(trueText);

  // Avoid infinite recursion if invokeJS fails during flutter_print
  if (_isPrintingText) {
    return;
  }
  
  _isPrintingText = true;
  try {
    await invokeJS("flutter_print", {"text": trueText});
  } finally {
    _isPrintingText = false;
  }
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
  final bool? peaks;

  Settings({
    this.output,
    this.scale,
    this.light,
    this.monitor,
    this.peaks
  });

  factory Settings.fromMap(Map<String, dynamic> map) {
    return Settings(
      output: map["output"],
      scale: map["scale"],
      light: map["light"],
      monitor: map["monitor"],
      peaks: map["peaks"]
    );
  }
}
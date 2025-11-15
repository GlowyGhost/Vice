import 'dart:html' as html;
import 'package:js/js_util.dart';
import 'dart:async';
import 'dart:convert';

final _ipcWaiters = <String, Completer<dynamic>>{};
final _ipcCmds = <String, String>{};

Future<dynamic> invokeJS(String cmd, [Map<String, dynamic>? args]) {
  final id = DateTime.now().microsecondsSinceEpoch.toString();

  final completer = Completer<dynamic>();
  _ipcWaiters[id] = completer;

  final payload = {
    "ipc_type": "request",
    "id": id,
    "cmd": cmd,
    "args": args ?? {},
  };

  _ipcCmds[id] = cmd;
  _sendToHost(payload);

  Timer(const Duration(seconds: 10), () {
    if (!completer.isCompleted) {
      _ipcWaiters.remove(id);
      completer.complete(null);
    }
  });

  return completer.future;
}

void initIpcListener() {
  html.window.onMessage.listen(_handleMessage);

  html.window.addEventListener('message', (event) {
    if (event is html.MessageEvent) _handleMessage(event);
  });
}

void _handleMessage(html.MessageEvent event) {
  final data = event.data;

  if (data is Map && data["ipc_type"] == "response") {
    final id = data["id"];
		var result = data["result"];

		if (result is String || result is bool) {
      result = result;
    } else if (result == null) {
      result = null;
    } else if (result is List) {
      result = result.map((e) {
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
      result = toMap(result);
    }

    _ipcCmds.remove(id);
    if (_ipcWaiters.containsKey(id)) {
      _ipcWaiters[id]!.complete(result);
      _ipcWaiters.remove(id);
    }
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

Future<void> printText(String? text) async {
  String trueText = "null";
  if (text != null) {
    trueText = text;
  }
  print(trueText);
  
  invokeJS("flutter_print", {"text": trueText});
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

void _sendToHost(dynamic payload) {
  try {
      final String jsonPayload = (() {
        try {
          return jsonEncode(payload);
        } catch (e) {
          try {
            if (payload is Map) {
              final simple = <String, dynamic>{};
              payload.forEach((k, v) {
                try {
                  simple[k.toString()] = v;
                } catch (_) {
                  simple[k.toString()] = v.toString();
                }
              });
              return jsonEncode(simple);
            }
          } catch (_) {}
          return '{}';
        }
      })();

      if (hasProperty(html.window, 'external') && hasProperty(getProperty(html.window, 'external'), 'invoke')) {
        final external = getProperty(html.window, 'external');
        callMethod(external, 'invoke', [jsonPayload]);

        return;
      }

      if (hasProperty(html.window, 'chrome')) {
        final chrome = getProperty(html.window, 'chrome');
        if (hasProperty(chrome, 'webview')) {
          final webview = getProperty(chrome, 'webview');
          callMethod(webview, 'postMessage', [jsonPayload]);

          return;
        }
      }

    if (hasProperty(html.window, 'ipc') && hasProperty(getProperty(html.window, 'ipc'), 'postMessage')) {
      final ipc = getProperty(html.window, 'ipc');
      callMethod(ipc, 'postMessage', [payload]);

      return;
    }
    html.window.postMessage(payload, '*');
  } catch (e) {
    print('IPC send failed: $e');
  }
}
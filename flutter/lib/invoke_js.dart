import 'package:js/js.dart';
import 'dart:js_util';
import 'dart:async';

@JS('window.__TAURI__.core.invoke')
external dynamic _invoke(String cmd, [dynamic args]);

Future<dynamic> invokeJS(String cmd, [Map<String, dynamic>? args]) async {
  try {
    final jsArgs = args != null ? jsify(args) : null;
    final promise = _invoke(cmd, jsArgs);
    final result = await promiseToFuture(promise);

    if (result is String) {
      return result;
    } else if (result == null) {
      return null;
    }

    return result;
  } catch (e) {
    return;
  }
}
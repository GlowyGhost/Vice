import 'dart:ui';
import 'dart:async';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'invoke_js.dart';
import 'randoms.dart';
import 'settings/page.dart';
import 'window.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();

  FlutterError.onError = (FlutterErrorDetails details) {
    FlutterError.dumpErrorToConsole(details);
    printText("FlutterError: ${details.exception}\n${details.stack}");
  };

  runZonedGuarded(() async {
    initIpcListener();

    try {
      await settings.loadSettings();
    } catch (e, st) {
      printText("Error loading settings: $e\n$st");
    }

    runApp(
      MultiProvider(
        providers: [
          ChangeNotifierProvider(create: (_) => AppStateNotifier()),
          ChangeNotifierProvider(create: (_) => settings),
        ],
        child: const App(),
      ),
    );
  }, (error, stack) {
    printText("Unhandled error: $error\n$stack");
  });
}

class App extends StatelessWidget {
  const App({super.key});

  @override
  Widget build(BuildContext context) {
    return Consumer2<AppStateNotifier, SettingsData>(
      builder: (context, appState, settingsData, _) {
        return MaterialApp(
          title: "Vice",
          theme: ThemeData(
            scaffoldBackgroundColor: bg_dark,
            primaryColor: bg_dark,
          ),
          home: const ScaledWindow(),
        );
      },
    );
  }
}

class ScaledWindow extends StatelessWidget {
  const ScaledWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return Consumer<AppStateNotifier>(
      builder: (context, appState, _) {
        double scale = clampDouble(settings.scale, 0.1, 2.0);
        if (scale > 2.0)  {
          scale = 2.0;
        } else if (scale < 0.1) {
          scale = 0.1;
        }

        return LayoutBuilder(
          builder: (context, constraints) {
            final parentW = constraints.maxWidth;
            final parentH = constraints.maxHeight;
            final childW = parentW / scale;
            final childH = parentH / scale;

            return SizedBox.expand(
              child: OverflowBox(
                minWidth: childW,
                maxWidth: childW,
                minHeight: childH,
                maxHeight: childH,
                alignment: Alignment.topLeft,
                child: Transform.scale(
                  scale: scale,
                  alignment: Alignment.topLeft,
                  child: const Window(),
                ),
              ),
            );
          },
        );
      },
    );
  }
}

class AppStateNotifier extends ChangeNotifier {
  void reload() {
    notifyListeners();
  }
}
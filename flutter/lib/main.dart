import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'invoke_js.dart';
import 'settings/page.dart';
import 'window.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  
  try {
    await settings.loadSettings();
  } catch (e) {
    printText("Error loading settings: $e");
  }

  runApp(
    ChangeNotifierProvider(
      create: (_) => AppStateNotifier(),
      child: const App(),
    ),
  );
}

class App extends StatelessWidget {
  const App({super.key});

  @override
  Widget build(BuildContext context) {
    return Consumer<AppStateNotifier>(
      builder: (context, appState, _) {
        return MaterialApp(
          title: "Vice",
          theme: ThemeData(
            scaffoldBackgroundColor: settings.lightMode
                ? const Color(0xFFCCCCCC)
                : const Color(0xFF262626),
            primaryColor: settings.lightMode
                ? const Color(0xFFCCCCCC)
                : const Color(0xFF262626),
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
        final scale = settings.scale;

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
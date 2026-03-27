import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:permission_handler/permission_handler.dart';
import 'bt_service.dart';
import 'session_provider.dart';
import 'screens/connect_screen.dart';
import 'screens/test_screen.dart';
import 'screens/history_screen.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await [
    Permission.bluetooth,
    Permission.bluetoothScan,
    Permission.bluetoothAdvertise,
    Permission.bluetoothConnect,
    Permission.location,
  ].request();

  runApp(
    MultiProvider(
      providers: [
        ChangeNotifierProvider(create: (_) => BtService()),
        ChangeNotifierProvider(
          create: (ctx) => SessionProvider(ctx.read<BtService>()),
        ),
      ],
      child: const TrainerLightsApp(),
    ),
  );
}

class TrainerLightsApp extends StatelessWidget {
  const TrainerLightsApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'TrainerLights',
      debugShowCheckedModeBanner: false,
      theme: lightTheme(),
      darkTheme: darkTheme(),
      themeMode: ThemeMode.system,
      initialRoute: '/',
      routes: {
        '/': (ctx) => const ConnectScreen(),
      },
      onGenerateRoute: (settings) {
        if (settings.name!.startsWith('/test/')) {
          final id = int.tryParse(settings.name!.split('/').last);
          if (id != null) {
            return MaterialPageRoute(
              builder: (ctx) => TestScreen(athleteId: id),
            );
          }
        } else if (settings.name!.startsWith('/history/')) {
          final id = int.tryParse(settings.name!.split('/').last);
          if (id != null) {
            return MaterialPageRoute(
              builder: (ctx) => HistoryScreen(athleteId: id),
            );
          }
        }
        return null;
      },
    );
  }

  static ThemeData lightTheme() => ThemeData(
    useMaterial3: true,
    colorSchemeSeed: const Color(0xFF007AFF),
    brightness: Brightness.light,
    cardTheme: CardThemeData(
      elevation: 0,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(16)),
      color: Colors.white,
      shadowColor: Colors.black12,
    ),
    appBarTheme: const AppBarTheme(
      backgroundColor: Color(0xFF1D1D1F),
      foregroundColor: Colors.white,
      elevation: 0,
      centerTitle: false,
      titleTextStyle: TextStyle(
        fontSize: 20, fontWeight: FontWeight.w800, color: Colors.white),
    ),
  );

  static ThemeData darkTheme() => ThemeData(
    useMaterial3: true,
    colorSchemeSeed: const Color(0xFF007AFF),
    brightness: Brightness.dark,
    cardTheme: CardThemeData(
      elevation: 0,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(16)),
      color: const Color(0xFF1C1C1E),
    ),
    appBarTheme: const AppBarTheme(
      backgroundColor: Color(0xFF1D1D1F),
      foregroundColor: Colors.white,
      elevation: 0,
      centerTitle: false,
      titleTextStyle: TextStyle(
        fontSize: 20, fontWeight: FontWeight.w800, color: Colors.white),
    ),
    scaffoldBackgroundColor: const Color(0xFF000000),
  );
}

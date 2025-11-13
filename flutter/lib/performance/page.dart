import 'dart:convert';
import 'package:fl_chart/fl_chart.dart';
import 'package:flutter/material.dart';
import 'package:vice/randoms.dart';
import 'dart:async';

import '../invoke_js.dart';

class Data {
  Map<String, List<double>> system;
  Map<String, List<double>> app;
  Map<String, double> general;

  Data(this.system, this.app, this.general);
}

class PerformancePage extends StatefulWidget {
  const PerformancePage({super.key});

  @override
  State<PerformancePage> createState() => _PerformancePageState();
}

class _PerformancePageState extends State<PerformancePage> {
  Data? data;
  bool _loading = false;
  Timer? _timer;

  @override
  void initState() {
    super.initState();
    _init();
    _timer = Timer.periodic(const Duration(seconds: 5), (_) => _init());
  }

  @override
  void dispose() {
    _timer?.cancel();
    super.dispose();
  }

  Future<void> _init() async {
    if (_loading) return;
    setState(() => _loading = true);

    try {
      final jsonString = await invokeJS("get_performance");
      if (jsonString == null) return;

      final Map<String, dynamic> dataDart = jsonDecode(jsonString);

      final systemRaw = (dataDart['system'] as Map<String, dynamic>);
      final appRaw = (dataDart['app'] as Map<String, dynamic>);
      final generalRaw = (dataDart['general'] as Map<String, dynamic>);

      final system = <String, List<double>>{};
      systemRaw.forEach((k, v) {
        system[k] = (v as List).map((e) => (e as num).toDouble()).toList();
      });

      final app = <String, List<double>>{};
      appRaw.forEach((k, v) {
        app[k] = (v as List).map((e) => (e as num).toDouble()).toList();
      });

      final general = <String, double>{};
      generalRaw.forEach((k, v) {
        general[k] = (v as num).toDouble();
      });

      system["cpu"] ??= List<double>.filled(5, 0);
      system["mem"] ??= List<double>.filled(5, 0);
      app["cpu"] ??= List<double>.filled(5, 0);
      app["ram"] ??= List<double>.filled(5, 0);
      general["ram"] = (general["ram"] == null || general["ram"] == 0) ? 1 : general["ram"]!;

      setState(() {
        data = Data(system, app, general);
      });
    } catch (e) {
      await printText('Failed to get performance data: $e');
    } finally {
      setState(() => _loading = false);
    }
  }

  Future<void> _clear() async {
    await invokeJS("clear_performance");
    _init();
  }

  @override
  Widget build(BuildContext context) {
    return Column(
      children: [
        Padding(
          padding: const EdgeInsets.only(top: 12, left: 8, bottom: 4, right: 8),
          child: Row(
            children: [
              ElevatedButton.icon(
                style: TextButton.styleFrom(
                  backgroundColor: bg_light,
                  foregroundColor: accent
                ),
                onPressed: _init,
                icon: const Icon(Icons.refresh),
                label: const Text("Refresh"),
              ),

              const SizedBox(width: 16),

              ElevatedButton.icon(
                style: TextButton.styleFrom(
                  backgroundColor: bg_light,
                  foregroundColor: accent
                ),
                onPressed: _clear,
                icon: const Icon(Icons.clear),
                label: const Text("Clear"),
              ),

              const Spacer(),
              Row(
                children: [
                  Row(
                    children: [
                      Container(color: Colors.blue, width: 30, height: 30),
                      const SizedBox(width: 8),
                      Text("Vice", style: TextStyle(fontSize: 18, color: text)),
                    ],
                  ),
                  const SizedBox(width: 12),
                  Row(
                    children: [
                      Container(color: Colors.red, width: 30, height: 30),
                      const SizedBox(width: 8),
                      Text("System", style: TextStyle(fontSize: 18, color: text)),
                    ],
                  ),
                ],
              )
            ],
          ),
        ),
        Expanded(
          child: _loading && data == null
            ? const Center(child: CircularProgressIndicator())
            : data == null
              ? Center(child: Text("No data", style: TextStyle(fontSize: 24, color: text)))
              : SingleChildScrollView(
                child: Padding(
                  padding: const EdgeInsets.all(16),
                  child: Column(
                    children: [
                      Text("CPU Usage", style: TextStyle(fontSize: 36, color: text)),

                      const SizedBox(height: 12),

                      SizedBox(
                        height: 220,
                        child: LineChart(
                          LineChartData(
                            minY: 0,
                            maxY: 100,
                            lineBarsData: [
                              _line((data!.system["cpu"] ?? []), Colors.red),
                              _line((data!.app["cpu"] ?? []), Colors.blue),
                            ],
                            titlesData: FlTitlesData(
                              bottomTitles: AxisTitles(sideTitles: SideTitles(showTitles: false)),
                              topTitles: AxisTitles(sideTitles: SideTitles(showTitles: false)),
                              rightTitles: AxisTitles(sideTitles: SideTitles(showTitles: false)),
                              leftTitles: AxisTitles(
                                sideTitles: SideTitles(
                                  showTitles: true,
                                  reservedSize: 40,
                                  getTitlesWidget: (value, meta) => Text("${value.toInt().toString()}%", style: TextStyle(fontSize: 10, color: text)),
                                ),
                              ),
                            ),
                            gridData: FlGridData(show: false),
                            borderData: FlBorderData(show: true),

                            lineTouchData: LineTouchData(
                              touchTooltipData: LineTouchTooltipData(
                                getTooltipItems: (touchedSpots) {
                                  return touchedSpots.map((spot) {
                                    return LineTooltipItem(
                                      "${spot.y.toStringAsFixed(1)}%",
                                      const TextStyle(color: Colors.white),
                                    );
                                  }).toList();
                                },
                              ),
                              handleBuiltInTouches: true,
                            ),
                          ),
                        ),
                      ),

                      const SizedBox(height: 32),

                      Text("RAM Usage", style: TextStyle(fontSize: 36, color: text)),

                      const SizedBox(height: 12),
                      
                      SizedBox(
                        height: 220,
                        child: LineChart(
                          LineChartData(
                            minY: 0,
                            maxY: (data!.general["ram"] ?? 1).toDouble() == 0 ? 1 : (data!.general["ram"] ?? 1).toDouble(),
                            lineBarsData: [
                              _line((data!.system["ram"] ?? []), Colors.red),
                              _line((data!.app["ram"] ?? []), Colors.blue),
                            ],
                            titlesData: FlTitlesData(
                              bottomTitles: AxisTitles(sideTitles: SideTitles(showTitles: false)),
                              topTitles: AxisTitles(sideTitles: SideTitles(showTitles: false)),
                              rightTitles: AxisTitles(sideTitles: SideTitles(showTitles: false)),
                              leftTitles: AxisTitles(
                                sideTitles: SideTitles(
                                  showTitles: true,
                                  reservedSize: 40,
                                  getTitlesWidget: (value, meta) => Text(value.toInt() >= 1000 ? "${(value.toInt() / 1000).toString()}GB" : "${value.toInt().toString()}MB", style: TextStyle(fontSize: 10, color: text)),
                                ),
                              ),
                            ),
                            gridData: FlGridData(show: false),
                            borderData: FlBorderData(show: true),

                            lineTouchData: LineTouchData(
                              touchTooltipData: LineTouchTooltipData(
                                getTooltipItems: (touchedSpots) {
                                  return touchedSpots.map((spot) {
                                    return LineTooltipItem(
                                      spot.y.toInt() >= 1000 ? "${(spot.y.toInt() / 1000).toStringAsFixed(1).toString()}GB" : "${spot.y.toInt().toStringAsFixed(1).toString()}MB",
                                      const TextStyle(color: Colors.white),
                                    );
                                  }).toList();
                                },
                              ),
                              handleBuiltInTouches: true,
                            ),
                          ),
                        ),
                      ),
                    ],
                  ),
                ),
              ),
        ),
      ],
    );
  }

  LineChartBarData _line(List<double> values, Color color) {
    final spots = <FlSpot>[];
    for (int i = 0; i < values.length; i++) {
      spots.add(FlSpot(i.toDouble(), values[i].toDouble()));
    }

    if (spots.isEmpty) spots.add(const FlSpot(0, 0));

    return LineChartBarData(
      spots: spots,
      isCurved: true,
      color: color,
      barWidth: 2,
      dotData: FlDotData(show: false),
    );
  }
}
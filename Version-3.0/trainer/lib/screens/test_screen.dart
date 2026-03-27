import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:fl_chart/fl_chart.dart';
import '../../session_provider.dart';
import '../../bt_service.dart';
import 'package:provider/provider.dart'; // For Consumer2
// Removed unused import

class TestScreen extends StatefulWidget {
  final int athleteId;
  const TestScreen({super.key, required this.athleteId});

  @override
  State<TestScreen> createState() => _TestScreenState();
}

class _TestScreenState extends State<TestScreen> {
  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) {
      context.read<SessionProvider>().requestNodeList();
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Training Session'),
        actions: [
          IconButton(
            icon: const Icon(Icons.refresh),
            onPressed: () => context.read<SessionProvider>().requestNodeList(),
          ),
        ],
      ),
      body: Consumer2<SessionProvider, BtService>(
        builder: (context, session, bt, child) {
          return Column(
            children: [
              // Connection Status
              Container(
                width: double.infinity,
                padding: const EdgeInsets.all(16),
                color: bt.isConnected ? Colors.green.shade100 : Colors.red.shade100,
                child: Row(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    Icon(
                      bt.isConnected ? Icons.bluetooth_connected : Icons.bluetooth_disabled,
                      color: bt.isConnected ? Colors.green : Colors.red,
                    ),
                    const SizedBox(width: 8),
                    Text(
                      bt.isConnected ? 'Connected' : 'Disconnected',
                      style: Theme.of(context).textTheme.titleMedium,
                    ),
                  ],
                ),
              ),
              // Live Stats Cards
              Expanded(
                child: Column(
                  children: [
                    // Top Stats Row
                    Padding(
                      padding: const EdgeInsets.all(16),
                      child: Row(
                        children: [
                          Expanded(child: _statCard('Hits', session.hits.toString(), Icons.check_circle)),
                           Expanded(child: _statCard('Misses', session.misses.toString(), Icons.error)),
                           Expanded(child: _statCard('Avg', '${session.avgMs}ms', Icons.speed)),
                        ],
                      ),
                    ),
                    // Response Time Chart
                    SizedBox(
                      height: 200,
                      child: session.responseSeries.isEmpty
                          ? const Center(child: Text('No data yet'))
                          : Padding(
                              padding: const EdgeInsets.all(16),
                              child: LineChart(
                                LineChartData(
                                  gridData: const FlGridData(show: false),
                                  titlesData: const FlTitlesData(show: false),
                                  borderData: FlBorderData(show: false),
                                  lineBarsData: [
                                    LineChartBarData(
                                      spots: session.responseSeries
                                          .asMap()
                                          .entries
                                          .map((e) => FlSpot(e.key.toDouble(), e.value))
                                          .toList(),
                                      isCurved: true,
                                      color: Colors.blue,
                                      barWidth: 3,
                                      belowBarData: BarAreaData(show: false),
                                    ),
                                  ],
                                ),
                              ),
                            ),
                    ),
                    // Node Grid
                    const Padding(
                      padding: EdgeInsets.symmetric(horizontal: 16),
                      child: Text('Nodes', style: TextStyle(fontWeight: FontWeight.bold, fontSize: 18)),
                    ),
                    Expanded(
                      child: GridView.builder(
                        padding: const EdgeInsets.all(16),
                        gridDelegate: const SliverGridDelegateWithFixedCrossAxisCount(
                          crossAxisCount: 2,
                          childAspectRatio: 2,
                          crossAxisSpacing: 8,
                          mainAxisSpacing: 8,
                        ),
                        itemCount: session.nodes.length,
                        itemBuilder: (ctx, idx) {
                          final node = session.nodes[idx];
                          return Container(
                            decoration: BoxDecoration(
                              color: node.isLive ? Colors.green.shade400 : Colors.red.shade400,
                              borderRadius: BorderRadius.circular(8),
                            ),
                            child: Column(
                              mainAxisAlignment: MainAxisAlignment.center,
                              children: [
                                Text(node.nodeId.toString(), style: const TextStyle(fontSize: 24, fontWeight: FontWeight.bold, color: Colors.white)),
                                Text(node.ip, style: const TextStyle(color: Colors.white70, fontSize: 12)),
                              ],
                            ),
                          );
                        },
                      ),
                    ),
                  ],
                ),
              ),
              // Controls
              Container(
                padding: const EdgeInsets.all(16),
                child: Row(
                  children: [
                    Expanded(
                      child: ElevatedButton.icon(
                        icon: const Icon(Icons.play_arrow),
                        label: const Text('Start'),
                        onPressed: () => context.read<SessionProvider>().startSession(widget.athleteId),
                      ),
                    ),
                    const SizedBox(width: 12),
                    Expanded(
                      child: ElevatedButton.icon(
                        icon: const Icon(Icons.stop),
                        label: const Text('Stop'),
                        style: ElevatedButton.styleFrom(backgroundColor: Colors.red),
                        onPressed: () => context.read<SessionProvider>().stopSession(),
                      ),
                    ),
                  ],
                ),
              ),
            ],
          );
        },
      ),
      floatingActionButton: FloatingActionButton(
        onPressed: () => context.read<SessionProvider>().pushConfig(),
        child: const Icon(Icons.settings),
      ),
    );
  }

  Widget _statCard(String label, String value, IconData icon) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          children: [
            Icon(icon, size: 32, color: Theme.of(context).colorScheme.primary),
            const SizedBox(height: 8),
            Text(value, style: Theme.of(context).textTheme.headlineMedium),
            Text(label, style: Theme.of(context).textTheme.bodyMedium),
          ],
        ),
      ),
    );
  }
}

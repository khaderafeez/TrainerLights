import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../../database_service.dart';
import '../../models/db_models.dart';
import '../../bt_service.dart';

class HistoryScreen extends StatefulWidget {
  final int athleteId;
  const HistoryScreen({super.key, required this.athleteId});

  @override
  State<HistoryScreen> createState() => _HistoryScreenState();
}

class _HistoryScreenState extends State<HistoryScreen> {
  List<Session> _sessions = [];
  bool _loading = true;

  @override
  void initState() {
    super.initState();
    _loadSessions();
  }

  Future<void> _loadSessions() async {
    final sessions = await DatabaseService.getSessionsByAthlete(widget.athleteId);
    setState(() {
      _sessions = sessions;
      _loading = false;
    });
  }

  @override
  Widget build(BuildContext context) {
    final bt = context.watch<BtService>();
    return Scaffold(
      appBar: AppBar(
        title: const Text('Session History'),
        actions: [
          if (bt.isConnected)
            IconButton(
              icon: const Icon(Icons.bluetooth_connected),
              onPressed: null,
            ),
        ],
      ),
      body: _loading
          ? const Center(child: CircularProgressIndicator())
          : RefreshIndicator(
              onRefresh: _loadSessions,
              child: _sessions.isEmpty
                  ? const Center(child: Text('No sessions yet'))
                  : ListView.builder(
                      itemCount: _sessions.length,
                      itemBuilder: (ctx, idx) {
                        final session = _sessions[idx];
                        return Card(
                          margin: const EdgeInsets.all(8),
                          child: ListTile(
                            title: Text(
                              session.startedAt.substring(11, 16), // HH:mm
                              style: const TextStyle(fontWeight: FontWeight.bold),
                            ),
                            subtitle: Column(
                              crossAxisAlignment: CrossAxisAlignment.start,
                              children: [
                                Text('${session.totalHits} hits / ${session.totalMisses} misses'),
                                Text('${session.avgResponseMs}ms avg (${session.bestResponseMs} best)'),
                                Text('Mode: ${session.mode}'),
                              ],
                            ),
                            trailing: const Icon(Icons.arrow_forward_ios),
                            onTap: () => _showSessionDetails(session),
                          ),
                        );
                      },
                    ),
            ),
    );
  }

  void _showSessionDetails(Session session) {
    showDialog(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text(session.startedAt),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('Hits: ${session.totalHits}'),
            Text('Misses: ${session.totalMisses}'),
            Text('Avg Response: ${session.avgResponseMs}ms'),
            Text('Best: ${session.bestResponseMs}ms'),
            Text('Worst: ${session.worstResponseMs}ms'),
            Text('Mode: ${session.mode}'),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx),
            child: const Text('Close'),
          ),
        ],
      ),
    );
  }
}

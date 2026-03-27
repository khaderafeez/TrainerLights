// lib/screens/connect_screen.dart
// ============================================================
// Initial screen for BT device connection and athlete selection
// ============================================================
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../bt_service.dart';
import '../session_provider.dart';
import '../database_service.dart';
import '../models/db_models.dart';

class ConnectScreen extends StatefulWidget {
  const ConnectScreen({super.key});

  @override
  State<ConnectScreen> createState() => _ConnectScreenState();
}

class _ConnectScreenState extends State<ConnectScreen> {
  List<Athlete> _athletes = [];
  bool _isLoading = false;

  @override
  void initState() {
    super.initState();
    _loadAthletes();
  }

  Future<void> _loadAthletes() async {
    setState(() => _isLoading = true);
    try {
      final athletes = await DatabaseService.getAllAthletes();
      setState(() => _athletes = athletes);
    } catch (e) {
      debugPrint('[ConnectScreen] Load athletes error: $e');
    } finally {
      setState(() => _isLoading = false);
    }
  }

  Future<void> _connectAndStart(Athlete athlete) async {
    final btService = context.read<BtService>();

    if (!btService.isConnected) {
      // Try to find and connect to device
      final device = await btService.findDevice();
      if (device == null) {
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(content: Text('TrainerLights device not found')),
          );
        }
        return;
      }

      final connected = await btService.connect(device);
      if (!connected) {
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(content: Text('Failed to connect to device')),
          );
        }
        return;
      }
    }

    // Start session
    if (mounted) {
      final sessionProvider = context.read<SessionProvider>();
      await sessionProvider.startSession(athlete.id ?? 0);
      if (athlete.id != null) {
        Navigator.of(context).pushReplacementNamed('/test/${athlete.id}');
      }
    }
  }

  Future<void> _addNewAthlete() async {
    final nameCtrl = TextEditingController();
    final disciplineCtrl = TextEditingController();

    final result = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('New Athlete'),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            TextField(
              controller: nameCtrl,
              decoration: const InputDecoration(labelText: 'Name'),
            ),
            const SizedBox(height: 16),
            TextField(
              controller: disciplineCtrl,
              decoration: const InputDecoration(labelText: 'Discipline'),
            ),
          ],
        ),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx, false), child: const Text('Cancel')),
          TextButton(
            onPressed: () => Navigator.pop(ctx, true),
            child: const Text('Add'),
          ),
        ],
      ),
    );

    if (result == true && nameCtrl.text.isNotEmpty) {
      final athlete = Athlete(
        name: nameCtrl.text,
        discipline: disciplineCtrl.text.isEmpty ? null : disciplineCtrl.text,
        createdAt: DateTime.now().toIso8601String(),
      );

      try {
        await DatabaseService.insertAthlete(athlete);
        await _loadAthletes();
      } catch (e) {
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(content: Text('Error adding athlete: $e'), duration: const Duration(seconds: 4)),
          );
        }
      }
    }

    nameCtrl.dispose();
    disciplineCtrl.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final btService = context.watch<BtService>();

    return Scaffold(
      appBar: AppBar(
        title: const Text('TrainerLights'),
        elevation: 0,
      ),
      body: _isLoading
          ? const Center(child: CircularProgressIndicator())
          : Column(
              children: [
                // Connection Status
                Container(
                  padding: const EdgeInsets.all(16),
                  child: Row(
                    children: [
                      Container(
                        width: 12,
                        height: 12,
                        decoration: BoxDecoration(
                          color: btService.isConnected ? Colors.green : Colors.red,
                          shape: BoxShape.circle,
                        ),
                      ),
                      const SizedBox(width: 12),
                      Text(
                        btService.isConnected ? 'Connected' : 'Disconnected',
                        style: const TextStyle(fontSize: 16),
                      ),
                    ],
                  ),
                ),
                Divider(height: 1, color: Colors.grey.withOpacity(0.3)),
                // Athletes List
                Expanded(
                  child: RefreshIndicator(
                    onRefresh: _loadAthletes,
                    child: _athletes.isEmpty
                      ? Center(
                          child: Column(
                            mainAxisAlignment: MainAxisAlignment.center,
                            children: [
                              const Text('No athletes registered'),
                              const SizedBox(height: 16),
                              ElevatedButton(
                                onPressed: _addNewAthlete,
                                child: const Text('Add Athlete'),
                              ),
                            ],
                          ),
                        )
                      : ListView.builder(
                          itemCount: _athletes.length,
                          itemBuilder: (ctx, idx) {
                            final athlete = _athletes[idx];
                            return ListTile(
                              title: Text(athlete.name),
                              subtitle: athlete.discipline != null ? Text(athlete.discipline!) : null,
                              trailing: const Icon(Icons.arrow_forward),
                              onTap: () => _connectAndStart(athlete),
                            );
                          },
                        ),
                  ),
                ),
                // Add Athlete FAB area
                Padding(
                  padding: const EdgeInsets.all(16),
                  child: ElevatedButton.icon(
                    onPressed: _addNewAthlete,
                    icon: const Icon(Icons.add),
                    label: const Text('New Athlete'),
                  ),
                ),
              ],
            ),
    );
  }
}

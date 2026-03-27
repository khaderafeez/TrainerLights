// lib/models/db_models.dart
// ============================================================
// Database models for session and athlete data
// ============================================================

class Session {
  final int?     id;
  final int      athleteId;
  final String   startedAt;
  final int      totalHits;
  final int      totalMisses;
  final int      avgResponseMs;
  final int      bestResponseMs;
  final int      worstResponseMs;
  final String   mode;
  final String?  endedAt;

  Session({
    this.id,
    required this.athleteId,
    required this.startedAt,
    required this.totalHits,
    required this.totalMisses,
    required this.avgResponseMs,
    required this.bestResponseMs,
    required this.worstResponseMs,
    required this.mode,
    this.endedAt,
  });

  Map<String, dynamic> toMap() {
    return {
      'id': id,
      'athleteId': athleteId,
      'startedAt': startedAt,
      'totalHits': totalHits,
      'totalMisses': totalMisses,
      'avgResponseMs': avgResponseMs,
      'bestResponseMs': bestResponseMs,
      'worstResponseMs': worstResponseMs,
      'mode': mode,
      'endedAt': endedAt,
    };
  }

  factory Session.fromMap(Map<String, dynamic> map) {
    return Session(
      id: map['id'],
      athleteId: map['athleteId'],
      startedAt: map['startedAt'],
      totalHits: map['totalHits'],
      totalMisses: map['totalMisses'],
      avgResponseMs: map['avgResponseMs'],
      bestResponseMs: map['bestResponseMs'],
      worstResponseMs: map['worstResponseMs'],
      mode: map['mode'],
      endedAt: map['endedAt'],
    );
  }
}

class Athlete {
  final int?     id;
  final String   name;
  final String?  discipline;
  final String?  createdAt;

  Athlete({
    this.id,
    required this.name,
    this.discipline,
    this.createdAt,
  });

  Map<String, dynamic> toMap() {
    return {
      'id': id,
      'name': name,
      'discipline': discipline,
      'createdAt': createdAt,
    };
  }

  factory Athlete.fromMap(Map<String, dynamic> map) {
    return Athlete(
      id: map['id'],
      name: map['name'],
      discipline: map['discipline'],
      createdAt: map['createdAt'],
    );
  }
}

// lib/database_service.dart
// ============================================================
// SQLite database service for persisting sessions and athletes
// ============================================================
import 'package:sqflite/sqflite.dart';
import 'package:path/path.dart';
import 'models/db_models.dart';

class DatabaseService {
  static Database? _db;

  static Future<Database> get database async {
    _db ??= await _initDatabase();
    return _db!;
  }

  static Future<Database> _initDatabase() async {
    final dbPath = await getDatabasesPath();
    final path = join(dbPath, 'trainerlights.db');

    return openDatabase(
      path,
      version: 1,
      onCreate: (db, version) {
        db.execute('''
          CREATE TABLE athletes (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            discipline TEXT,
            createdAt TEXT
          )
        ''');
        db.execute('''
          CREATE TABLE sessions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            athleteId INTEGER NOT NULL,
            startedAt TEXT NOT NULL,
            endedAt TEXT,
            totalHits INTEGER,
            totalMisses INTEGER,
            avgResponseMs INTEGER,
            bestResponseMs INTEGER,
            worstResponseMs INTEGER,
            mode TEXT,
            FOREIGN KEY(athleteId) REFERENCES athletes(id)
          )
        ''');
      },
    );
  }

  // ---- Athlete operations ----
  static Future<int> insertAthlete(Athlete athlete) async {
    final db = await database;
    return db.insert('athletes', athlete.toMap());
  }

  static Future<List<Athlete>> getAllAthletes() async {
    final db = await database;
    final maps = await db.query('athletes');
    return maps.map((map) => Athlete.fromMap(map)).toList();
  }

  static Future<Athlete?> getAthlete(int id) async {
    final db = await database;
    final maps = await db.query('athletes', where: 'id = ?', whereArgs: [id]);
    if (maps.isNotEmpty) {
      return Athlete.fromMap(maps.first);
    }
    return null;
  }

  static Future<int> updateAthlete(Athlete athlete) async {
    final db = await database;
    return db.update('athletes', athlete.toMap(), where: 'id = ?', whereArgs: [athlete.id]);
  }

  static Future<int> deleteAthlete(int id) async {
    final db = await database;
    return db.delete('athletes', where: 'id = ?', whereArgs: [id]);
  }

  // ---- Session operations ----
  static Future<int> insertSession(Session session) async {
    final db = await database;
    return db.insert('sessions', session.toMap());
  }

  static Future<List<Session>> getSessionsByAthlete(int athleteId) async {
    final db = await database;
    final maps = await db.query(
      'sessions',
      where: 'athleteId = ?',
      whereArgs: [athleteId],
      orderBy: 'startedAt DESC',
    );
    return maps.map((map) => Session.fromMap(map)).toList();
  }

  static Future<Session?> getSession(int id) async {
    final db = await database;
    final maps = await db.query('sessions', where: 'id = ?', whereArgs: [id]);
    if (maps.isNotEmpty) {
      return Session.fromMap(maps.first);
    }
    return null;
  }

  static Future<int> updateSession(Session session) async {
    final db = await database;
    return db.update('sessions', session.toMap(), where: 'id = ?', whereArgs: [session.id]);
  }

  static Future<int> deleteSession(int id) async {
    final db = await database;
    return db.delete('sessions', where: 'id = ?', whereArgs: [id]);
  }

  static Future<void> close() async {
    _db?.close();
    _db = null;
  }
}

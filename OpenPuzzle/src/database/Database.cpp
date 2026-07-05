#include "openpuzzle/database/Database.hpp"
#include <iostream>
namespace openpuzzle {
Database::~Database() { close(); }
bool Database::open(const std::string &path) {
  close();
  return sqlite3_open(path.c_str(), &db_) == SQLITE_OK;
}
void Database::close() {
  if (db_) {
    sqlite3_close(db_);
    db_ = nullptr;
  }
}
bool Database::exec(const std::string &sql) {
  char *err = nullptr;
  int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err);
  if (rc != SQLITE_OK) {
    if (err) {
      std::cerr << "SQLite error: " << err << "\n";
      sqlite3_free(err);
    }
    return false;
  }
  return true;
}
bool Database::createSchema() {
  bool ok = exec(R"SQL(
PRAGMA foreign_keys=ON;
CREATE TABLE IF NOT EXISTS puzzles(id INTEGER PRIMARY KEY AUTOINCREMENT,number INTEGER NOT NULL UNIQUE,name TEXT NOT NULL,address TEXT NOT NULL,hash160 TEXT DEFAULT '',range_start TEXT NOT NULL,range_end TEXT NOT NULL,reward REAL DEFAULT 0,solved INTEGER DEFAULT 0,solved_key TEXT DEFAULT '',solved_address TEXT DEFAULT '',sharing TEXT DEFAULT 'private',created_at TEXT DEFAULT CURRENT_TIMESTAMP);
CREATE TABLE IF NOT EXISTS ranges(id INTEGER PRIMARY KEY AUTOINCREMENT,puzzle_id INTEGER NOT NULL,start_key TEXT NOT NULL,end_key TEXT NOT NULL,block_bits INTEGER NOT NULL DEFAULT 0,status INTEGER NOT NULL DEFAULT 1,keys_checked TEXT DEFAULT '0',allocator_version TEXT DEFAULT 'foundation',created_at TEXT DEFAULT CURRENT_TIMESTAMP,reserved_at TEXT,started_at TEXT,finished_at TEXT,UNIQUE(puzzle_id,start_key,end_key),FOREIGN KEY(puzzle_id) REFERENCES puzzles(id));
CREATE TABLE IF NOT EXISTS jobs(id INTEGER PRIMARY KEY AUTOINCREMENT,puzzle_id INTEGER NOT NULL,range_id INTEGER NOT NULL,state INTEGER NOT NULL DEFAULT 1,created_at TEXT DEFAULT CURRENT_TIMESTAMP,started_at TEXT,finished_at TEXT,FOREIGN KEY(puzzle_id) REFERENCES puzzles(id),FOREIGN KEY(range_id) REFERENCES ranges(id));
CREATE TABLE IF NOT EXISTS executions(id INTEGER PRIMARY KEY AUTOINCREMENT,job_id INTEGER NOT NULL,workspace TEXT NOT NULL,command TEXT NOT NULL,state TEXT NOT NULL,exit_code INTEGER,started_at TEXT DEFAULT CURRENT_TIMESTAMP,finished_at TEXT,FOREIGN KEY(job_id) REFERENCES jobs(id));
CREATE TABLE IF NOT EXISTS statistics(id INTEGER PRIMARY KEY AUTOINCREMENT,execution_id INTEGER NOT NULL,timestamp TEXT DEFAULT CURRENT_TIMESTAMP,speed_mkeys REAL,temperature_c REAL,power_w REAL,FOREIGN KEY(execution_id) REFERENCES executions(id));
CREATE TABLE IF NOT EXISTS external_ranges(id INTEGER PRIMARY KEY AUTOINCREMENT,puzzle_id INTEGER NOT NULL,start_key TEXT NOT NULL,end_key TEXT NOT NULL,source TEXT,confidence TEXT,notes TEXT,imported_at TEXT DEFAULT CURRENT_TIMESTAMP,FOREIGN KEY(puzzle_id) REFERENCES puzzles(id));
CREATE TABLE IF NOT EXISTS gpu_profiles(id INTEGER PRIMARY KEY AUTOINCREMENT,gpu_name TEXT NOT NULL,backend TEXT NOT NULL,engine TEXT NOT NULL,blocks INTEGER NOT NULL,threads INTEGER NOT NULL,points INTEGER NOT NULL,average_speed REAL NOT NULL,minimum_speed REAL DEFAULT 0,maximum_speed REAL DEFAULT 0,samples INTEGER DEFAULT 0,created_at TEXT DEFAULT CURRENT_TIMESTAMP,UNIQUE(gpu_name,backend,engine));
)SQL");

  char *migrationError = nullptr;
  sqlite3_exec(db_,
               "ALTER TABLE ranges ADD COLUMN keys_checked TEXT DEFAULT '0'",
               nullptr, nullptr, &migrationError);

  if (migrationError) {
    sqlite3_free(migrationError);
  }

  migrationError = nullptr;
  sqlite3_exec(db_, "ALTER TABLE puzzles ADD COLUMN hash160 TEXT DEFAULT ''",
               nullptr, nullptr, &migrationError);
  if (migrationError) sqlite3_free(migrationError);

  migrationError = nullptr;
  sqlite3_exec(db_, "ALTER TABLE puzzles ADD COLUMN solved INTEGER DEFAULT 0",
               nullptr, nullptr, &migrationError);
  if (migrationError) sqlite3_free(migrationError);

  migrationError = nullptr;
  sqlite3_exec(db_, "ALTER TABLE puzzles ADD COLUMN solved_key TEXT DEFAULT ''",
               nullptr, nullptr, &migrationError);
  if (migrationError) sqlite3_free(migrationError);

  migrationError = nullptr;
  sqlite3_exec(db_, "ALTER TABLE puzzles ADD COLUMN solved_address TEXT DEFAULT ''",
               nullptr, nullptr, &migrationError);
  if (migrationError) sqlite3_free(migrationError);

  return ok;
}
bool Database::upsertPuzzle(const PuzzleRecord &p) {
  const char *sql =
      "INSERT INTO "
      "puzzles(number,name,address,hash160,range_start,range_end,reward,solved,"
      "solved_key,solved_address,sharing) "
      "VALUES(?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT(number) DO UPDATE SET "
      "name=excluded.name,"
      "address=excluded.address,"
      "hash160=excluded.hash160,"
      "range_start=excluded.range_start,"
      "range_end=excluded.range_end,"
      "reward=excluded.reward,"
      "solved=excluded.solved,"
      "solved_key=excluded.solved_key,"
      "solved_address=excluded.solved_address,"
      "sharing=excluded.sharing;";
  sqlite3_stmt *s = nullptr;
  sqlite3_prepare_v2(db_, sql, -1, &s, nullptr);
  sqlite3_bind_int(s, 1, p.number);
  sqlite3_bind_text(s, 2, p.name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 3, p.address.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 4, p.hash160.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 5, p.rangeStart.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 6, p.rangeEnd.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_double(s, 7, p.reward);
  sqlite3_bind_int(s, 8, p.solved ? 1 : 0);
  sqlite3_bind_text(s, 9, p.solvedKey.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 10, p.solvedAddress.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 11, p.sharing.c_str(), -1, SQLITE_TRANSIENT);
  bool ok = sqlite3_step(s) == SQLITE_DONE;
  sqlite3_finalize(s);
  return ok;
}

bool Database::updatePuzzleHash160(int number, const std::string &hash160) {
  sqlite3_stmt *s = nullptr;
  sqlite3_prepare_v2(db_, "UPDATE puzzles SET hash160=? WHERE number=?", -1, &s,
                     nullptr);
  sqlite3_bind_text(s, 1, hash160.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(s, 2, number);
  bool ok = sqlite3_step(s) == SQLITE_DONE;
  sqlite3_finalize(s);
  return ok;
}
std::optional<PuzzleRecord> Database::getPuzzleByNumber(int number) {
  const char *sql =
      "SELECT id,number,name,address,COALESCE(hash160,''),range_start,range_end,"
      "reward,COALESCE(solved,0),COALESCE(solved_key,''),"
      "COALESCE(solved_address,''),sharing FROM puzzles WHERE number=?";
  sqlite3_stmt *s = nullptr;
  sqlite3_prepare_v2(db_, sql, -1, &s, nullptr);
  sqlite3_bind_int(s, 1, number);
  std::optional<PuzzleRecord> out;
  if (sqlite3_step(s) == SQLITE_ROW) {
    PuzzleRecord p;
    p.id = sqlite3_column_int(s, 0);
    p.number = sqlite3_column_int(s, 1);
    p.name = (const char *)sqlite3_column_text(s, 2);
    p.address = (const char *)sqlite3_column_text(s, 3);
    p.hash160 = (const char *)sqlite3_column_text(s, 4);
    p.rangeStart = (const char *)sqlite3_column_text(s, 5);
    p.rangeEnd = (const char *)sqlite3_column_text(s, 6);
    p.reward = sqlite3_column_double(s, 7);
    p.solved = sqlite3_column_int(s, 8) != 0;
    p.solvedKey = (const char *)sqlite3_column_text(s, 9);
    p.solvedAddress = (const char *)sqlite3_column_text(s, 10);
    p.sharing = (const char *)sqlite3_column_text(s, 11);
    out = p;
  }
  sqlite3_finalize(s);
  return out;
}
std::vector<PuzzleRecord> Database::listPuzzles() {
  std::vector<PuzzleRecord> v;
  sqlite3_stmt *s = nullptr;
  sqlite3_prepare_v2(
      db_,
      "SELECT id,number,name,address,COALESCE(hash160,''),range_start,range_end,"
      "reward,COALESCE(solved,0),COALESCE(solved_key,''),"
      "COALESCE(solved_address,''),sharing FROM puzzles ORDER BY number",
      -1, &s, nullptr);
  while (sqlite3_step(s) == SQLITE_ROW) {
    PuzzleRecord p;
    p.id = sqlite3_column_int(s, 0);
    p.number = sqlite3_column_int(s, 1);
    p.name = (const char *)sqlite3_column_text(s, 2);
    p.address = (const char *)sqlite3_column_text(s, 3);
    p.hash160 = (const char *)sqlite3_column_text(s, 4);
    p.rangeStart = (const char *)sqlite3_column_text(s, 5);
    p.rangeEnd = (const char *)sqlite3_column_text(s, 6);
    p.reward = sqlite3_column_double(s, 7);
    p.solved = sqlite3_column_int(s, 8) != 0;
    p.solvedKey = (const char *)sqlite3_column_text(s, 9);
    p.solvedAddress = (const char *)sqlite3_column_text(s, 10);
    p.sharing = (const char *)sqlite3_column_text(s, 11);
    v.push_back(p);
  }
  sqlite3_finalize(s);
  return v;
}
int Database::insertRange(const RangeRecord &r) {
  const char *sql =
      "INSERT OR IGNORE INTO "
      "ranges(puzzle_id,start_key,end_key,block_bits,status,keys_checked,"
      "reserved_at) VALUES(?,?,?,?,?,?,CURRENT_TIMESTAMP)";
  sqlite3_stmt *s = nullptr;
  sqlite3_prepare_v2(db_, sql, -1, &s, nullptr);
  sqlite3_bind_int(s, 1, r.puzzleId);
  sqlite3_bind_text(s, 2, r.startKey.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 3, r.endKey.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(s, 4, r.blockBits);
  sqlite3_bind_int(s, 5, (int)r.status);
  sqlite3_bind_text(s, 6, r.keysChecked.c_str(), -1, SQLITE_TRANSIENT);
  bool ok = sqlite3_step(s) == SQLITE_DONE;
  sqlite3_finalize(s);
  return ok ? (int)sqlite3_last_insert_rowid(db_) : 0;
}
std::vector<RangeRecord> Database::listRanges(int puzzleId) {
  std::vector<RangeRecord> v;
  sqlite3_stmt *s = nullptr;
  sqlite3_prepare_v2(db_,
                     "SELECT "
                     "id,puzzle_id,start_key,end_key,block_bits,status,"
                     "COALESCE(keys_checked,\'0\') "
                     "FROM ranges WHERE puzzle_id=? ORDER BY start_key",
                     -1, &s, nullptr);
  sqlite3_bind_int(s, 1, puzzleId);
  while (sqlite3_step(s) == SQLITE_ROW) {
    RangeRecord r;
    r.id = sqlite3_column_int(s, 0);
    r.puzzleId = sqlite3_column_int(s, 1);
    r.startKey = (const char *)sqlite3_column_text(s, 2);
    r.endKey = (const char *)sqlite3_column_text(s, 3);
    r.blockBits = sqlite3_column_int(s, 4);
    r.status = (RangeStatus)sqlite3_column_int(s, 5);
    if (sqlite3_column_count(s) > 6 && sqlite3_column_text(s, 6)) {
      r.keysChecked = (const char *)sqlite3_column_text(s, 6);
    }
    v.push_back(r);
  }
  sqlite3_finalize(s);
  return v;
}
std::optional<RangeRecord> Database::getRange(int id) {
  sqlite3_stmt *s = nullptr;
  sqlite3_prepare_v2(db_,
                     "SELECT "
                     "id,puzzle_id,start_key,end_key,block_bits,status,"
                     "COALESCE(keys_checked,\'0\') "
                     "FROM ranges WHERE id=?",
                     -1, &s, nullptr);
  sqlite3_bind_int(s, 1, id);
  std::optional<RangeRecord> out;
  if (sqlite3_step(s) == SQLITE_ROW) {
    RangeRecord r;
    r.id = sqlite3_column_int(s, 0);
    r.puzzleId = sqlite3_column_int(s, 1);
    r.startKey = (const char *)sqlite3_column_text(s, 2);
    r.endKey = (const char *)sqlite3_column_text(s, 3);
    r.blockBits = sqlite3_column_int(s, 4);
    r.status = (RangeStatus)sqlite3_column_int(s, 5);
    if (sqlite3_column_count(s) > 6 && sqlite3_column_text(s, 6)) {
      r.keysChecked = (const char *)sqlite3_column_text(s, 6);
    }
    out = r;
  }
  sqlite3_finalize(s);
  return out;
}
bool Database::updateRangeStatus(int id, RangeStatus st) {
  sqlite3_stmt *s = nullptr;
  sqlite3_prepare_v2(
      db_,
      "UPDATE ranges SET status=?,started_at=CASE WHEN ?=2 THEN "
      "CURRENT_TIMESTAMP ELSE started_at END,finished_at=CASE WHEN ? IN "
      "(3,4,5) THEN CURRENT_TIMESTAMP ELSE finished_at END WHERE id=?",
      -1, &s, nullptr);
  int val = (int)st;
  sqlite3_bind_int(s, 1, val);
  sqlite3_bind_int(s, 2, val);
  sqlite3_bind_int(s, 3, val);
  sqlite3_bind_int(s, 4, id);
  bool ok = sqlite3_step(s) == SQLITE_DONE;
  sqlite3_finalize(s);
  return ok;
}
int Database::insertJob(const JobRecord &j) {
  sqlite3_stmt *s = nullptr;
  sqlite3_prepare_v2(db_,
                     "INSERT INTO jobs(puzzle_id,range_id,state) VALUES(?,?,?)",
                     -1, &s, nullptr);
  sqlite3_bind_int(s, 1, j.puzzleId);
  sqlite3_bind_int(s, 2, j.rangeId);
  sqlite3_bind_int(s, 3, (int)j.state);
  bool ok = sqlite3_step(s) == SQLITE_DONE;
  sqlite3_finalize(s);
  return ok ? (int)sqlite3_last_insert_rowid(db_) : 0;
}
std::optional<JobRecord> Database::getJob(int id) {
  sqlite3_stmt *s = nullptr;
  sqlite3_prepare_v2(db_,
                     "SELECT id,puzzle_id,range_id,state FROM jobs WHERE id=?",
                     -1, &s, nullptr);
  sqlite3_bind_int(s, 1, id);
  std::optional<JobRecord> out;
  if (sqlite3_step(s) == SQLITE_ROW) {
    JobRecord j;
    j.id = sqlite3_column_int(s, 0);
    j.puzzleId = sqlite3_column_int(s, 1);
    j.rangeId = sqlite3_column_int(s, 2);
    j.state = (JobState)sqlite3_column_int(s, 3);
    out = j;
  }
  sqlite3_finalize(s);
  return out;
}
bool Database::updateJobState(int id, JobState st) {
  sqlite3_stmt *s = nullptr;
  sqlite3_prepare_v2(
      db_,
      "UPDATE jobs SET state=?,started_at=CASE WHEN ?=2 THEN CURRENT_TIMESTAMP "
      "ELSE started_at END,finished_at=CASE WHEN ? IN (3,4,5) THEN "
      "CURRENT_TIMESTAMP ELSE finished_at END WHERE id=?",
      -1, &s, nullptr);
  int val = (int)st;
  sqlite3_bind_int(s, 1, val);
  sqlite3_bind_int(s, 2, val);
  sqlite3_bind_int(s, 3, val);
  sqlite3_bind_int(s, 4, id);
  bool ok = sqlite3_step(s) == SQLITE_DONE;
  sqlite3_finalize(s);
  return ok;
}
int Database::insertExecution(int jobId, const std::string &ws,
                              const std::string &cmd,
                              const std::string &state) {
  sqlite3_stmt *s = nullptr;
  sqlite3_prepare_v2(
      db_,
      "INSERT INTO executions(job_id,workspace,command,state) VALUES(?,?,?,?)",
      -1, &s, nullptr);
  sqlite3_bind_int(s, 1, jobId);
  sqlite3_bind_text(s, 2, ws.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 3, cmd.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 4, state.c_str(), -1, SQLITE_TRANSIENT);
  bool ok = sqlite3_step(s) == SQLITE_DONE;
  sqlite3_finalize(s);
  return ok ? (int)sqlite3_last_insert_rowid(db_) : 0;
}
bool Database::finishExecution(int id, const std::string &state, int code) {
  sqlite3_stmt *s = nullptr;
  sqlite3_prepare_v2(
      db_,
      "UPDATE executions SET state=?,exit_code=?,finished_at=CURRENT_TIMESTAMP "
      "WHERE id=?",
      -1, &s, nullptr);
  sqlite3_bind_text(s, 1, state.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(s, 2, code);
  sqlite3_bind_int(s, 3, id);
  bool ok = sqlite3_step(s) == SQLITE_DONE;
  sqlite3_finalize(s);
  return ok;
}
int Database::insertStatistic(int executionId, double speed, double temp,
                              double power) {
  sqlite3_stmt *s = nullptr;
  sqlite3_prepare_v2(
      db_,
      "INSERT INTO statistics(execution_id,speed_mkeys,temperature_c,power_w) "
      "VALUES(?,?,?,?)",
      -1, &s, nullptr);
  sqlite3_bind_int(s, 1, executionId);
  sqlite3_bind_double(s, 2, speed);
  sqlite3_bind_double(s, 3, temp);
  sqlite3_bind_double(s, 4, power);
  bool ok = sqlite3_step(s) == SQLITE_DONE;
  sqlite3_finalize(s);
  return ok ? (int)sqlite3_last_insert_rowid(db_) : 0;
}

bool Database::updateRangeKeysChecked(int rangeId,
                                      const std::string &keysChecked) {
  sqlite3_stmt *s = nullptr;

  sqlite3_prepare_v2(db_, "UPDATE ranges SET keys_checked=? WHERE id=?", -1, &s,
                     nullptr);

  sqlite3_bind_text(s, 1, keysChecked.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(s, 2, rangeId);

  bool ok = sqlite3_step(s) == SQLITE_DONE;

  sqlite3_finalize(s);

  return ok;
}

bool Database::insertExternalRange(int puzzleId, const std::string &start,
                                   const std::string &end,
                                   const std::string &source,
                                   const std::string &confidence,
                                   const std::string &notes) {
  sqlite3_stmt *s = nullptr;
  sqlite3_prepare_v2(db_,
                     "INSERT INTO "
                     "external_ranges(puzzle_id,start_key,end_key,source,"
                     "confidence,notes) VALUES(?,?,?,?,?,?)",
                     -1, &s, nullptr);
  sqlite3_bind_int(s, 1, puzzleId);
  sqlite3_bind_text(s, 2, start.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 3, end.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 4, source.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 5, confidence.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 6, notes.c_str(), -1, SQLITE_TRANSIENT);
  bool ok = sqlite3_step(s) == SQLITE_DONE;
  sqlite3_finalize(s);
  return ok;
}
std::vector<RangeRecord> Database::listExternalRanges(int puzzleId) {
  std::vector<RangeRecord> v;
  sqlite3_stmt *s = nullptr;
  sqlite3_prepare_v2(db_,
                     "SELECT id,puzzle_id,start_key,end_key FROM "
                     "external_ranges WHERE puzzle_id=? ORDER BY start_key",
                     -1, &s, nullptr);
  sqlite3_bind_int(s, 1, puzzleId);
  while (sqlite3_step(s) == SQLITE_ROW) {
    RangeRecord r;
    r.id = sqlite3_column_int(s, 0);
    r.puzzleId = sqlite3_column_int(s, 1);
    r.startKey = (const char *)sqlite3_column_text(s, 2);
    r.endKey = (const char *)sqlite3_column_text(s, 3);
    r.status = RangeStatus::External;
    v.push_back(r);
  }
  sqlite3_finalize(s);
  return v;
}
long long Database::countRangesByStatus(int puzzleId, RangeStatus status) {
  sqlite3_stmt *s = nullptr;
  sqlite3_prepare_v2(
      db_, "SELECT COUNT(*) FROM ranges WHERE puzzle_id=? AND status=?", -1, &s,
      nullptr);
  sqlite3_bind_int(s, 1, puzzleId);
  sqlite3_bind_int(s, 2, (int)status);
  long long c = 0;
  if (sqlite3_step(s) == SQLITE_ROW)
    c = sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return c;
}
long long Database::countJobsByState(int puzzleId, JobState state) {
  sqlite3_stmt *s = nullptr;
  sqlite3_prepare_v2(db_,
                     "SELECT COUNT(*) FROM jobs WHERE puzzle_id=? AND state=?",
                     -1, &s, nullptr);
  sqlite3_bind_int(s, 1, puzzleId);
  sqlite3_bind_int(s, 2, (int)state);
  long long c = 0;
  if (sqlite3_step(s) == SQLITE_ROW)
    c = sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return c;
}
} // namespace openpuzzle

namespace openpuzzle {

bool Database::saveGpuProfile(const GpuProfileRecord &profile) {
  const char *sql =
      "INSERT INTO gpu_profiles(gpu_name,backend,engine,blocks,threads,points,"
      "average_speed,minimum_speed,maximum_speed,samples) "
      "VALUES(?,?,?,?,?,?,?,?,?,?) "
      "ON CONFLICT(gpu_name,backend,engine) DO UPDATE SET "
      "blocks=excluded.blocks,"
      "threads=excluded.threads,"
      "points=excluded.points,"
      "average_speed=excluded.average_speed,"
      "minimum_speed=excluded.minimum_speed,"
      "maximum_speed=excluded.maximum_speed,"
      "samples=excluded.samples,"
      "created_at=CURRENT_TIMESTAMP;";

  sqlite3_stmt *stmt = nullptr;

  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return false;

  sqlite3_bind_text(stmt, 1, profile.gpuName.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, profile.backend.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, profile.engine.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 4, profile.blocks);
  sqlite3_bind_int(stmt, 5, profile.threads);
  sqlite3_bind_int(stmt, 6, profile.points);
  sqlite3_bind_double(stmt, 7, profile.averageSpeed);
  sqlite3_bind_double(stmt, 8, profile.minimumSpeed);
  sqlite3_bind_double(stmt, 9, profile.maximumSpeed);
  sqlite3_bind_int(stmt, 10, profile.samples);

  bool ok = sqlite3_step(stmt) == SQLITE_DONE;
  sqlite3_finalize(stmt);

  return ok;
}

std::optional<GpuProfileRecord>
Database::getGpuProfile(const std::string &gpuName, const std::string &backend,
                        const std::string &engine) {
  const char *sql =
      "SELECT id,gpu_name,backend,engine,blocks,threads,points,average_speed,"
      "minimum_speed,maximum_speed,samples "
      "FROM gpu_profiles WHERE gpu_name=? AND backend=? AND engine=?;";

  sqlite3_stmt *stmt = nullptr;

  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return std::nullopt;

  sqlite3_bind_text(stmt, 1, gpuName.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, backend.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, engine.c_str(), -1, SQLITE_TRANSIENT);

  std::optional<GpuProfileRecord> out;

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    GpuProfileRecord profile;

    profile.id = sqlite3_column_int(stmt, 0);
    profile.gpuName = (const char *)sqlite3_column_text(stmt, 1);
    profile.backend = (const char *)sqlite3_column_text(stmt, 2);
    profile.engine = (const char *)sqlite3_column_text(stmt, 3);
    profile.blocks = sqlite3_column_int(stmt, 4);
    profile.threads = sqlite3_column_int(stmt, 5);
    profile.points = sqlite3_column_int(stmt, 6);
    profile.averageSpeed = sqlite3_column_double(stmt, 7);
    profile.minimumSpeed = sqlite3_column_double(stmt, 8);
    profile.maximumSpeed = sqlite3_column_double(stmt, 9);
    profile.samples = sqlite3_column_int(stmt, 10);

    out = profile;
  }

  sqlite3_finalize(stmt);

  return out;
}

} // namespace openpuzzle

namespace openpuzzle {

std::vector<GpuProfileRecord> Database::listGpuProfiles() {
  std::vector<GpuProfileRecord> profiles;

  const char *sql =
      "SELECT id,gpu_name,backend,engine,blocks,threads,points,average_speed,"
      "minimum_speed,maximum_speed,samples "
      "FROM gpu_profiles ORDER BY average_speed DESC;";

  sqlite3_stmt *stmt = nullptr;

  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
    return profiles;

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    GpuProfileRecord profile;

    profile.id = sqlite3_column_int(stmt, 0);
    profile.gpuName = (const char *)sqlite3_column_text(stmt, 1);
    profile.backend = (const char *)sqlite3_column_text(stmt, 2);
    profile.engine = (const char *)sqlite3_column_text(stmt, 3);
    profile.blocks = sqlite3_column_int(stmt, 4);
    profile.threads = sqlite3_column_int(stmt, 5);
    profile.points = sqlite3_column_int(stmt, 6);
    profile.averageSpeed = sqlite3_column_double(stmt, 7);
    profile.minimumSpeed = sqlite3_column_double(stmt, 8);
    profile.maximumSpeed = sqlite3_column_double(stmt, 9);
    profile.samples = sqlite3_column_int(stmt, 10);

    profiles.push_back(profile);
  }

  sqlite3_finalize(stmt);

  return profiles;
}

} // namespace openpuzzle

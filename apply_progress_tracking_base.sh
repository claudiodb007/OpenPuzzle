#!/usr/bin/env bash
set -euo pipefail

python3 - <<'PY'
from pathlib import Path

# 1) Models.hpp
p = Path("OpenPuzzle/include/openpuzzle/models/Models.hpp")
txt = p.read_text()

if "keysChecked" not in txt:
    txt = txt.replace(
        "RangeStatus status = RangeStatus::Reserved;",
        'RangeStatus status = RangeStatus::Reserved;\n  std::string keysChecked = "0";'
    )

p.write_text(txt)

# 2) BitCrackOutputParser.hpp
p = Path("OpenPuzzle/include/openpuzzle/adapters/bitcrack/BitCrackOutputParser.hpp")
txt = p.read_text()

if "totalKeys" not in txt:
    txt = txt.replace(
        "double speedMKeys = 0.0;",
        'double speedMKeys = 0.0;\n  std::string totalKeys;'
    )

p.write_text(txt)

# 3) BitCrackOutputParser.cpp
p = Path("OpenPuzzle/src/adapters/bitcrack/BitCrackOutputParser.cpp")
txt = p.read_text()

old = '''      result.type = ParsedLineType::Speed;
      result.value = match[1].str();
      result.speedMKeys = std::stod(result.value);
      return result;'''

new = '''      result.type = ParsedLineType::Speed;
      result.value = match[1].str();
      result.speedMKeys = std::stod(result.value);

      std::regex totalRegex(R"\\\\((([0-9,]+)\\\\s+total)\\\\)", std::regex::icase);
      std::smatch totalMatch;
      if (std::regex_search(line, totalMatch, totalRegex)) {
        result.totalKeys = totalMatch[1].str();
        result.totalKeys.erase(
            std::remove(result.totalKeys.begin(), result.totalKeys.end(), ','),
            result.totalKeys.end());
      }

      return result;'''

if old not in txt:
    raise SystemExit("Bloco speed parser não encontrado")

txt = txt.replace(old, new, 1)
p.write_text(txt)

# 4) Database.cpp schema + selects/inserts
p = Path("OpenPuzzle/src/database/Database.cpp")
txt = p.read_text()

if "ALTER TABLE ranges ADD COLUMN keys_checked" not in txt:
    txt = txt.replace(
        ")SQL\");\n}",
        ''')SQL") &&
         exec("ALTER TABLE ranges ADD COLUMN keys_checked TEXT DEFAULT '0'");\n}'''
    )

# If ALTER TABLE fails because column exists, current exec() returns false.
# So replace with SQLite-compatible ignore via PRAGMA check later by simple callback is too big.
# Instead we will undo this fragile change below if needed.
if "ALTER TABLE ranges ADD COLUMN keys_checked TEXT DEFAULT '0'" in txt:
    txt = txt.replace(
        '''  return exec(R"SQL(
PRAGMA foreign_keys=ON;
CREATE TABLE IF NOT EXISTS puzzles(id INTEGER PRIMARY KEY AUTOINCREMENT,number INTEGER NOT NULL UNIQUE,name TEXT NOT NULL,address TEXT NOT NULL,range_start TEXT NOT NULL,range_end TEXT NOT NULL,reward REAL DEFAULT 0,sharing TEXT DEFAULT 'private',created_at TEXT DEFAULT CURRENT_TIMESTAMP);
CREATE TABLE IF NOT EXISTS ranges(id INTEGER PRIMARY KEY AUTOINCREMENT,puzzle_id INTEGER NOT NULL,start_key TEXT NOT NULL,end_key TEXT NOT NULL,block_bits INTEGER NOT NULL DEFAULT 0,status INTEGER NOT NULL DEFAULT 1,allocator_version TEXT DEFAULT 'foundation',created_at TEXT DEFAULT CURRENT_TIMESTAMP,reserved_at TEXT,started_at TEXT,finished_at TEXT,UNIQUE(puzzle_id,start_key,end_key),FOREIGN KEY(puzzle_id) REFERENCES puzzles(id));
CREATE TABLE IF NOT EXISTS jobs(id INTEGER PRIMARY KEY AUTOINCREMENT,puzzle_id INTEGER NOT NULL,range_id INTEGER NOT NULL,state INTEGER NOT NULL DEFAULT 1,created_at TEXT DEFAULT CURRENT_TIMESTAMP,started_at TEXT,finished_at TEXT,FOREIGN KEY(puzzle_id) REFERENCES puzzles(id),FOREIGN KEY(range_id) REFERENCES ranges(id));
CREATE TABLE IF NOT EXISTS executions(id INTEGER PRIMARY KEY AUTOINCREMENT,job_id INTEGER NOT NULL,workspace TEXT NOT NULL,command TEXT NOT NULL,state TEXT NOT NULL,exit_code INTEGER,started_at TEXT DEFAULT CURRENT_TIMESTAMP,finished_at TEXT,FOREIGN KEY(job_id) REFERENCES jobs(id));
CREATE TABLE IF NOT EXISTS statistics(id INTEGER PRIMARY KEY AUTOINCREMENT,execution_id INTEGER NOT NULL,timestamp TEXT DEFAULT CURRENT_TIMESTAMP,speed_mkeys REAL,temperature_c REAL,power_w REAL,FOREIGN KEY(execution_id) REFERENCES executions(id));
CREATE TABLE IF NOT EXISTS external_ranges(id INTEGER PRIMARY KEY AUTOINCREMENT,puzzle_id INTEGER NOT NULL,start_key TEXT NOT NULL,end_key TEXT NOT NULL,source TEXT,confidence TEXT,notes TEXT,imported_at TEXT DEFAULT CURRENT_TIMESTAMP,FOREIGN KEY(puzzle_id) REFERENCES puzzles(id));
)SQL") &&
         exec("ALTER TABLE ranges ADD COLUMN keys_checked TEXT DEFAULT '0'");
}''',
        '''  bool ok = exec(R"SQL(
PRAGMA foreign_keys=ON;
CREATE TABLE IF NOT EXISTS puzzles(id INTEGER PRIMARY KEY AUTOINCREMENT,number INTEGER NOT NULL UNIQUE,name TEXT NOT NULL,address TEXT NOT NULL,range_start TEXT NOT NULL,range_end TEXT NOT NULL,reward REAL DEFAULT 0,sharing TEXT DEFAULT 'private',created_at TEXT DEFAULT CURRENT_TIMESTAMP);
CREATE TABLE IF NOT EXISTS ranges(id INTEGER PRIMARY KEY AUTOINCREMENT,puzzle_id INTEGER NOT NULL,start_key TEXT NOT NULL,end_key TEXT NOT NULL,block_bits INTEGER NOT NULL DEFAULT 0,status INTEGER NOT NULL DEFAULT 1,keys_checked TEXT DEFAULT '0',allocator_version TEXT DEFAULT 'foundation',created_at TEXT DEFAULT CURRENT_TIMESTAMP,reserved_at TEXT,started_at TEXT,finished_at TEXT,UNIQUE(puzzle_id,start_key,end_key),FOREIGN KEY(puzzle_id) REFERENCES puzzles(id));
CREATE TABLE IF NOT EXISTS jobs(id INTEGER PRIMARY KEY AUTOINCREMENT,puzzle_id INTEGER NOT NULL,range_id INTEGER NOT NULL,state INTEGER NOT NULL DEFAULT 1,created_at TEXT DEFAULT CURRENT_TIMESTAMP,started_at TEXT,finished_at TEXT,FOREIGN KEY(puzzle_id) REFERENCES puzzles(id),FOREIGN KEY(range_id) REFERENCES ranges(id));
CREATE TABLE IF NOT EXISTS executions(id INTEGER PRIMARY KEY AUTOINCREMENT,job_id INTEGER NOT NULL,workspace TEXT NOT NULL,command TEXT NOT NULL,state TEXT NOT NULL,exit_code INTEGER,started_at TEXT DEFAULT CURRENT_TIMESTAMP,finished_at TEXT,FOREIGN KEY(job_id) REFERENCES jobs(id));
CREATE TABLE IF NOT EXISTS statistics(id INTEGER PRIMARY KEY AUTOINCREMENT,execution_id INTEGER NOT NULL,timestamp TEXT DEFAULT CURRENT_TIMESTAMP,speed_mkeys REAL,temperature_c REAL,power_w REAL,FOREIGN KEY(execution_id) REFERENCES executions(id));
CREATE TABLE IF NOT EXISTS external_ranges(id INTEGER PRIMARY KEY AUTOINCREMENT,puzzle_id INTEGER NOT NULL,start_key TEXT NOT NULL,end_key TEXT NOT NULL,source TEXT,confidence TEXT,notes TEXT,imported_at TEXT DEFAULT CURRENT_TIMESTAMP,FOREIGN KEY(puzzle_id) REFERENCES puzzles(id));
)SQL");

  exec("ALTER TABLE ranges ADD COLUMN keys_checked TEXT DEFAULT '0'");

  return ok;
}'''
    )

txt = txt.replace(
    '"SELECT id,puzzle_id,start_key,end_key,block_bits,status "',
    '"SELECT id,puzzle_id,start_key,end_key,block_bits,status,COALESCE(keys_checked,\\\'0\\\') "'
)

txt = txt.replace(
    "r.status = (RangeStatus)sqlite3_column_int(s, 5);",
    '''r.status = (RangeStatus)sqlite3_column_int(s, 5);
    if (sqlite3_column_count(s) > 6 && sqlite3_column_text(s, 6)) {
      r.keysChecked = (const char *)sqlite3_column_text(s, 6);
    }'''
)

txt = txt.replace(
    '''"ranges(puzzle_id,start_key,end_key,block_bits,status,"
                    "reserved_at) VALUES(?,?,?,?,?,CURRENT_TIMESTAMP)"''',
    '''"ranges(puzzle_id,start_key,end_key,block_bits,status,keys_checked,"
                    "reserved_at) VALUES(?,?,?,?,?,?,CURRENT_TIMESTAMP)"'''
)

txt = txt.replace(
    "sqlite3_bind_int(s, 5, (int)r.status);\n  bool ok = sqlite3_step(s) == SQLITE_DONE;",
    '''sqlite3_bind_int(s, 5, (int)r.status);
  sqlite3_bind_text(s, 6, r.keysChecked.c_str(), -1, SQLITE_TRANSIENT);
  bool ok = sqlite3_step(s) == SQLITE_DONE;'''
)

p.write_text(txt)
PY

cat > OpenPuzzle/tests/adapters/BitCrackParserTotalTests.cpp <<'EOF2'
#include "openpuzzle/adapters/bitcrack/BitCrackOutputParser.hpp"

using namespace openpuzzle::bitcrack;

int main()
{
    BitCrackOutputParser parser;

    auto parsed = parser.parse(
        "NVIDIA GeForce R 7918 / 11873MB | 1 target 1319.71 MKey/s (104,958,263,296 total) [00:01:18]"
    );

    if (parsed.type != ParsedLineType::Speed) return 1;
    if (parsed.speedMKeys != 1319.71) return 2;
    if (parsed.totalKeys != "104958263296") return 3;

    return 0;
}
EOF2

python3 - <<'PY'
from pathlib import Path

p = Path("OpenPuzzle/tests/CMakeLists.txt")
txt = p.read_text()

if "BitCrackParserTotalTests" not in txt:
    txt += """

add_executable(BitCrackParserTotalTests
    adapters/BitCrackParserTotalTests.cpp
)

target_link_libraries(BitCrackParserTotalTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME BitCrackParserTotalTests
    COMMAND BitCrackParserTotalTests
)
"""

p.write_text(txt)
PY

cd OpenPuzzle
./scripts/format.sh
./scripts/test.sh

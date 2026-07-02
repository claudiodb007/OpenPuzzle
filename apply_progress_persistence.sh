#!/usr/bin/env bash
set -euo pipefail

python3 - <<'PY'
from pathlib import Path

# 1) ExecutionResult.hpp
p = Path("OpenPuzzle/include/openpuzzle/core/ExecutionResult.hpp")
txt = p.read_text()

if "#include <string>" not in txt:
    txt = txt.replace("#pragma once", "#pragma once\n\n#include <string>")

if "keysChecked" not in txt:
    txt = txt.replace(
        "double averageSpeed = 0.0;",
        'double averageSpeed = 0.0;\n  std::string keysChecked = "0";'
    )

p.write_text(txt)

# 2) ExecutionManager.cpp: capturar totalKeys
p = Path("OpenPuzzle/src/core/ExecutionManager.cpp")
txt = p.read_text()

if "parsed.totalKeys" not in txt:
    txt = txt.replace(
        '''        if (parsed.type == bitcrack::ParsedLineType::Speed) {
          result.averageSpeed = parsed.speedMKeys;
        }''',
        '''        if (parsed.type == bitcrack::ParsedLineType::Speed) {
          result.averageSpeed = parsed.speedMKeys;

          if (!parsed.totalKeys.empty()) {
            result.keysChecked = parsed.totalKeys;
          }
        }'''
    )

# state.json RUNNING inicial
txt = txt.replace(
    '''      stateFile << "  \\"average_speed\\": 0\\n";''',
    '''      stateFile << "  \\"average_speed\\": 0,\\n";
      stateFile << "  \\"keys_checked\\": \\"0\\"\\n";'''
)

# state.json final
txt = txt.replace(
    '''      stateFile << "  \\"average_speed\\": " << result.averageSpeed << "\\n";''',
    '''      stateFile << "  \\"average_speed\\": " << result.averageSpeed << ",\\n";
      stateFile << "  \\"keys_checked\\": \\"" << result.keysChecked << "\\"\\n";'''
)

p.write_text(txt)

# 3) Database.cpp: implementar updateRangeKeysChecked se faltar
p = Path("OpenPuzzle/src/database/Database.cpp")
txt = p.read_text()

if "Database::updateRangeKeysChecked" not in txt:
    insert_before = "bool Database::insertExternalRange"
    func = r'''
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

'''
    if insert_before not in txt:
        raise SystemExit("Não encontrei local para inserir updateRangeKeysChecked")
    txt = txt.replace(insert_before, func + insert_before)

p.write_text(txt)

# 4) Scheduler.cpp: persistir keysChecked depois da execução
p = Path("OpenPuzzle/src/core/Scheduler.cpp")
txt = p.read_text()

if "executionResult.keysChecked" not in txt:
    txt = txt.replace(
        '''  db.finishExecution(executionId,
                     executionResult.success ? "finished" : "failed",
                     executionResult.exitCode);''',
        '''  if (!executionResult.keysChecked.empty()) {
    db.updateRangeKeysChecked(range.id, executionResult.keysChecked);
  }

  db.finishExecution(executionId,
                     executionResult.success ? "finished" : "failed",
                     executionResult.exitCode);'''
    )

p.write_text(txt)
PY

cat > OpenPuzzle/tests/core/ExecutionProgressPersistenceTests.cpp <<'CPP'
#include "openpuzzle/core/ExecutionContext.hpp"
#include "openpuzzle/core/ExecutionManager.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace openpuzzle;

static std::string readFile(const std::filesystem::path &path)
{
    std::ifstream in(path);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

int main()
{
    auto temp = std::filesystem::temp_directory_path() / "openpuzzle_progress_persistence_test";
    std::filesystem::remove_all(temp);
    std::filesystem::create_directories(temp);

    ExecutionContext ctx;
    ctx.executionId = 1;
    ctx.puzzleId = 71;
    ctx.jobId = 42;
    ctx.rangeId = 1001;
    ctx.engine = "BitCrack";
    ctx.workspace = temp.string();
    ctx.command = "printf 'NVIDIA GeForce R 7918 / 11873MB | 1 target 1319.71 MKey/s (104,958,263,296 total) [00:01:18]\\n'";
    ctx.echoOutput = false;

    ExecutionManager manager;
    auto result = manager.run(ctx);

    if (!result.success) return 1;
    if (result.averageSpeed != 1319.71) return 2;
    if (result.keysChecked != "104958263296") return 3;

    auto state = readFile(temp / "state.json");

    if (state.find("\"keys_checked\": \"104958263296\"") == std::string::npos) return 4;

    std::filesystem::remove_all(temp);

    return 0;
}
CPP

python3 - <<'PY'
from pathlib import Path

p = Path("OpenPuzzle/tests/CMakeLists.txt")
txt = p.read_text()

if "ExecutionProgressPersistenceTests" not in txt:
    txt += """

add_executable(ExecutionProgressPersistenceTests
    core/ExecutionProgressPersistenceTests.cpp
)

target_link_libraries(ExecutionProgressPersistenceTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ExecutionProgressPersistenceTests
    COMMAND ExecutionProgressPersistenceTests
)
"""

p.write_text(txt)
PY

cd OpenPuzzle
./scripts/format.sh
./scripts/test.sh

#!/usr/bin/env bash
set -euo pipefail

python3 - <<'PY'
from pathlib import Path

# 1) ExecutionContext.hpp: adicionar callback de progresso
p = Path("OpenPuzzle/include/openpuzzle/core/ExecutionContext.hpp")
txt = p.read_text()

if "#include <functional>" not in txt:
    txt = txt.replace("#include <string>", "#include <functional>\n#include <string>")

if "ExecutionResult" not in txt:
    txt = txt.replace("namespace openpuzzle {", "namespace openpuzzle {\n\nstruct ExecutionResult;")

if "onProgress" not in txt:
    txt = txt.replace(
        "bool echoOutput = true;",
        "bool echoOutput = true;\n\n  std::function<void(const ExecutionResult &)> onProgress;"
    )

p.write_text(txt)

# 2) Scheduler.hpp: runExistingJob passa context por valor
p = Path("OpenPuzzle/include/openpuzzle/core/Scheduler.hpp")
txt = p.read_text()

txt = txt.replace(
    "const ExecutionContext &context,\n                                 const ExecutionManager &executionManager,",
    "ExecutionContext context,\n                                 const ExecutionManager &executionManager,"
)

p.write_text(txt)

# 3) Scheduler.cpp: runExistingJob passa context por valor e grava progresso live
p = Path("OpenPuzzle/src/core/Scheduler.cpp")
txt = p.read_text()

txt = txt.replace(
    "const ExecutionContext &context, const ExecutionManager &executionManager,",
    "ExecutionContext context, const ExecutionManager &executionManager,"
)

old = '''  db.updateJobState(job.id, JobState::Running);
  db.updateRangeStatus(range.id, RangeStatus::Running);

  auto executionResult = executionManager.run(context);'''

new = '''  db.updateJobState(job.id, JobState::Running);
  db.updateRangeStatus(range.id, RangeStatus::Running);

  context.onProgress = [&](const ExecutionResult &progress) {
    if (!progress.keysChecked.empty()) {
      db.updateRangeKeysChecked(range.id, progress.keysChecked);
    }
  };

  auto executionResult = executionManager.run(context);'''

if old not in txt:
    raise SystemExit("Bloco runExistingJob não encontrado")

txt = txt.replace(old, new, 1)
p.write_text(txt)
PY

cat > OpenPuzzle/src/core/ExecutionManager.cpp <<'EOF2'
#include "openpuzzle/core/ExecutionManager.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace openpuzzle {

static void writeStateFile(const ExecutionContext &context,
                           const std::string &status,
                           const ExecutionResult &result) {
  if (context.workspace.empty()) {
    return;
  }

  std::ofstream stateFile(std::filesystem::path(context.workspace) /
                          "state.json");

  if (!stateFile.is_open()) {
    return;
  }

  stateFile << "{\n";
  stateFile << "  \"status\": \"" << status << "\",\n";
  stateFile << "  \"exit_code\": " << result.exitCode << ",\n";
  stateFile << "  \"lines_read\": " << result.linesRead << ",\n";
  stateFile << "  \"average_speed\": " << result.averageSpeed << ",\n";
  stateFile << "  \"keys_checked\": \"" << result.keysChecked << "\"\n";
  stateFile << "}\n";
}

ExecutionSummary ExecutionManager::runCommand(const std::string &command,
                                              bool echoOutput) const {
  ExecutionSummary summary;

  ProcessRunner runner;
  bitcrack::BitCrackOutputParser parser;

  auto result = runner.run(command, [&](const std::string &line) {
    summary.totalLines++;

    if (echoOutput) {
      std::cout << line << "\n";
    }

    auto parsed = parser.parse(line);

    switch (parsed.type) {
    case bitcrack::ParsedLineType::Speed:
      summary.speedEvents++;
      summary.lastSpeedMKeys = parsed.speedMKeys;
      break;
    case bitcrack::ParsedLineType::Error:
      summary.errorEvents++;
      break;
    case bitcrack::ParsedLineType::Found:
      summary.foundEvents++;
      break;
    case bitcrack::ParsedLineType::Finished:
      summary.finishedEvents++;
      break;
    default:
      break;
    }
  });

  summary.started = result.started;
  summary.exitCode = result.exitCode;

  return summary;
}

ExecutionResult ExecutionManager::run(const ExecutionContext &context) const {
  ExecutionResult result;
  result.exitCode = -1;
  result.keysChecked = "0";

  std::ofstream stdoutLog;

  if (!context.workspace.empty()) {
    std::filesystem::create_directories(context.workspace);

    stdoutLog.open(std::filesystem::path(context.workspace) / "stdout.log",
                   std::ios::app);

    std::ofstream executionFile(std::filesystem::path(context.workspace) /
                                "execution.json");

    if (executionFile.is_open()) {
      executionFile << "{\n";
      executionFile << "  \"execution_id\": " << context.executionId << ",\n";
      executionFile << "  \"puzzle_id\": " << context.puzzleId << ",\n";
      executionFile << "  \"job_id\": " << context.jobId << ",\n";
      executionFile << "  \"range_id\": " << context.rangeId << ",\n";
      executionFile << "  \"engine\": \"" << context.engine << "\",\n";
      executionFile << "  \"command\": \"" << context.command << "\",\n";
      executionFile << "  \"workspace\": \"" << context.workspace << "\",\n";
      executionFile << "  \"echo_output\": "
                    << (context.echoOutput ? "true" : "false") << "\n";
      executionFile << "}\n";
    }

    writeStateFile(context, "RUNNING", result);
  }

  ProcessRunner runner;
  bitcrack::BitCrackOutputParser parser;

  auto processResult =
      runner.run(context.command, [&](const std::string &line) {
        result.linesRead++;

        if (context.echoOutput) {
          std::cout << line << "\n";
        }

        if (stdoutLog.is_open()) {
          stdoutLog << line << "\n";
          stdoutLog.flush();
        }

        auto parsed = parser.parse(line);

        if (parsed.type == bitcrack::ParsedLineType::Speed) {
          result.averageSpeed = parsed.speedMKeys;

          if (!parsed.totalKeys.empty()) {
            result.keysChecked = parsed.totalKeys;
          }

          writeStateFile(context, "RUNNING", result);

          if (context.onProgress) {
            context.onProgress(result);
          }
        }

        if (parsed.type == bitcrack::ParsedLineType::Found) {
          result.keyFound = true;
          result.privateKey = parsed.value;
        }
      });

  result.exitCode = processResult.exitCode;
  result.success = processResult.started && processResult.exitCode == 0;

  writeStateFile(context, result.success ? "FINISHED" : "FAILED", result);

  return result;
}

} // namespace openpuzzle
EOF2

cat > OpenPuzzle/tests/core/ExecutionLiveProgressTests.cpp <<'EOF2'
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
    auto temp = std::filesystem::temp_directory_path() / "openpuzzle_live_progress_test";
    std::filesystem::remove_all(temp);
    std::filesystem::create_directories(temp);

    int progressEvents = 0;
    std::string lastKeysChecked;

    ExecutionContext ctx;
    ctx.executionId = 1;
    ctx.puzzleId = 71;
    ctx.jobId = 42;
    ctx.rangeId = 1001;
    ctx.engine = "BitCrack";
    ctx.workspace = temp.string();
    ctx.command =
        "printf 'NVIDIA GeForce R 7918 / 11873MB | 1 target 1319.71 MKey/s (104,958,263,296 total) [00:01:18]\\n'";
    ctx.echoOutput = false;
    ctx.onProgress = [&](const ExecutionResult &progress) {
        progressEvents++;
        lastKeysChecked = progress.keysChecked;
    };

    ExecutionManager manager;
    auto result = manager.run(ctx);

    if (!result.success) return 1;
    if (progressEvents != 1) return 2;
    if (lastKeysChecked != "104958263296") return 3;

    auto state = readFile(temp / "state.json");

    if (state.find("\"status\": \"FINISHED\"") == std::string::npos) return 4;
    if (state.find("\"keys_checked\": \"104958263296\"") == std::string::npos) return 5;

    std::filesystem::remove_all(temp);

    return 0;
}
EOF2

python3 - <<'PY'
from pathlib import Path

p = Path("OpenPuzzle/tests/CMakeLists.txt")
txt = p.read_text()

if "ExecutionLiveProgressTests" not in txt:
    txt += """

add_executable(ExecutionLiveProgressTests
    core/ExecutionLiveProgressTests.cpp
)

target_link_libraries(ExecutionLiveProgressTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ExecutionLiveProgressTests
    COMMAND ExecutionLiveProgressTests
)
"""

p.write_text(txt)
PY

cd OpenPuzzle
./scripts/format.sh
./scripts/test.sh

#!/usr/bin/env bash
set -euo pipefail

python3 - <<'PY'
from pathlib import Path

p = Path("OpenPuzzle/include/openpuzzle/core/Scheduler.hpp")
txt = p.read_text()

if "#include <string>" not in txt:
    txt = txt.replace("#pragma once", "#pragma once\n\n#include <string>")

if "workspaceForJob" not in txt:
    txt = txt.replace(
        "ExecutionContext buildExecutionContext(int executionId, int puzzleId,",
        """std::string workspaceForJob(int jobId) const;

  std::string buildBitCrackCommand(const std::string &bitcrackPath,
                                   const PuzzleRecord &puzzle,
                                   const RangeRecord &range, int device,
                                   int blocks, int threads, int points,
                                   const std::string &outputFile) const;

  ExecutionContext buildExecutionContext(int executionId, int puzzleId,"""
    )

p.write_text(txt)
PY

python3 - <<'PY'
from pathlib import Path

p = Path("OpenPuzzle/src/core/Scheduler.cpp")
txt = p.read_text()

if "#include <filesystem>" not in txt:
    txt = txt.replace(
        '#include "openpuzzle/core/Scheduler.hpp"',
        '#include "openpuzzle/core/Scheduler.hpp"\n\n#include <cstdlib>\n#include <filesystem>\n#include <iomanip>\n#include <sstream>'
    )

if "Scheduler::workspaceForJob" not in txt:
    txt = txt.replace(
        "SchedulerResult\nScheduler::runOnce",
        """std::string Scheduler::workspaceForJob(int jobId) const {
  const char *home = std::getenv("HOME");
  std::filesystem::path path =
      home ? std::filesystem::path(home) : std::filesystem::current_path();

  std::ostringstream id;
  id << std::setw(6) << std::setfill('0') << jobId;

  path /= ".local/share/OpenPuzzle/jobs";
  path /= id.str();

  std::filesystem::create_directories(path);

  return path.string();
}

std::string Scheduler::buildBitCrackCommand(
    const std::string &bitcrackPath, const PuzzleRecord &puzzle,
    const RangeRecord &range, int device, int blocks, int threads, int points,
    const std::string &outputFile) const {
  std::ostringstream command;

  command << bitcrackPath << " " << puzzle.address << " --keyspace "
          << range.startKey << ":" << range.endKey << " --out " << outputFile
          << " -d " << device << " -b " << blocks << " -t " << threads
          << " -p " << points;

  return command.str();
}

SchedulerResult
Scheduler::runOnce"""
    )

p.write_text(txt)
PY

cat > OpenPuzzle/tests/core/SchedulerCommandBuilderTests.cpp <<'CPP'
#include "openpuzzle/core/Scheduler.hpp"

#include <filesystem>

using namespace openpuzzle;

int main()
{
    Scheduler scheduler;

    auto workspace = scheduler.workspaceForJob(42);

    if (workspace.find("000042") == std::string::npos) return 1;

    PuzzleRecord puzzle;
    puzzle.address = "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU";

    RangeRecord range;
    range.startKey = "400000000000000000";
    range.endKey = "4000000000000000FF";

    auto output = (std::filesystem::path(workspace) / "found.txt").string();

    auto command = scheduler.buildBitCrackCommand(
        "/usr/local/bin/cuBitCrack",
        puzzle,
        range,
        0,
        256,
        256,
        1024,
        output
    );

    if (command.find("/usr/local/bin/cuBitCrack") == std::string::npos) return 2;
    if (command.find("1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU") == std::string::npos) return 3;
    if (command.find("--keyspace 400000000000000000:4000000000000000FF") == std::string::npos) return 4;
    if (command.find("--out ") == std::string::npos) return 5;
    if (command.find("-d 0") == std::string::npos) return 6;
    if (command.find("-b 256") == std::string::npos) return 7;
    if (command.find("-t 256") == std::string::npos) return 8;
    if (command.find("-p 1024") == std::string::npos) return 9;

    return 0;
}
CPP

python3 - <<'PY'
from pathlib import Path

p = Path("OpenPuzzle/tests/CMakeLists.txt")
txt = p.read_text()

if "SchedulerCommandBuilderTests" not in txt:
    txt += """

add_executable(SchedulerCommandBuilderTests
    core/SchedulerCommandBuilderTests.cpp
)

target_link_libraries(SchedulerCommandBuilderTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME SchedulerCommandBuilderTests
    COMMAND SchedulerCommandBuilderTests
)
"""

p.write_text(txt)
PY

cd OpenPuzzle
./scripts/format.sh
./scripts/test.sh

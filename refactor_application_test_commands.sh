#!/usr/bin/env bash
set -euo pipefail

# Run from repository root:
# /home/claudiodb/Secretária/OpenPuzzle/OpenPuzzle_0.12_github_ready

if [ ! -d "OpenPuzzle" ]; then
  echo "ERROR: run this from the repo root that contains the OpenPuzzle/ directory."
  exit 1
fi

mkdir -p OpenPuzzle/src/core/commands

cat > OpenPuzzle/src/core/commands/ApplicationTestCommands.cpp <<'EOF'
#include "openpuzzle/core/Application.hpp"
#include "openpuzzle/core/EventBus.hpp"
#include "openpuzzle/core/ExecutionManager.hpp"
#include "openpuzzle/core/ExecutionSession.hpp"
#include "openpuzzle/core/ProcessRunner.hpp"

#include <iostream>
#include <string>

namespace openpuzzle {

int Application::cmdProcessTest(const std::vector<std::string>& args) {
    std::string command = getArg(args, "--command", "printf 'line1\\nline2\\n'");

    ProcessRunner runner;

    auto result = runner.run(command, [](const std::string& line) {
        std::cout << "OUT.................. " << line << "\\n";
    });

    std::cout << "Started.............. " << (result.started ? "yes" : "no") << "\\n";
    std::cout << "Exit code............ " << result.exitCode << "\\n";

    return result.exitCode == 0 ? 0 : 1;
}

int Application::cmdExecutionTest(const std::vector<std::string>& args) {
    std::string command = getArg(
        args,
        "--command",
        "printf '[Info] Starting at: 400000000000000000\\n[Info] 1334.62 MKey/s\\nFinished\\n'"
    );

    ExecutionManager manager;
    auto summary = manager.runCommand(command, true);

    std::cout << "Started.............. " << (summary.started ? "yes" : "no") << "\\n";
    std::cout << "Exit code............ " << summary.exitCode << "\\n";
    std::cout << "Lines................ " << summary.totalLines << "\\n";
    std::cout << "Speed events......... " << summary.speedEvents << "\\n";
    std::cout << "Last speed........... " << summary.lastSpeedMKeys << " MKey/s\\n";
    std::cout << "Error events......... " << summary.errorEvents << "\\n";
    std::cout << "Found events......... " << summary.foundEvents << "\\n";
    std::cout << "Finished events...... " << summary.finishedEvents << "\\n";

    return summary.started && summary.exitCode == 0 ? 0 : 1;
}

int Application::cmdSessionTest(const std::vector<std::string>& args) {
    (void)args;

    ExecutionSession session;
    session.executionId = 1;
    session.jobId = 42;
    session.puzzleId = 71;
    session.rangeId = 1001;
    session.engine = "BitCrack";
    session.workspace = "~/.local/share/OpenPuzzle/jobs/000042";
    session.command = "cuBitCrack ...";
    session.status = ExecutionSessionStatus::Created;

    std::cout << "Execution ID......... " << session.executionId << "\\n";
    std::cout << "Job ID............... " << session.jobId << "\\n";
    std::cout << "Puzzle ID............ " << session.puzzleId << "\\n";
    std::cout << "Range ID............. " << session.rangeId << "\\n";
    std::cout << "Engine............... " << session.engine << "\\n";
    std::cout << "Workspace............ " << session.workspace << "\\n";
    std::cout << "Status............... " << ExecutionSession::statusToString(session.status) << "\\n";

    return 0;
}

int Application::cmdEventTest(const std::vector<std::string>& args) {
    (void)args;

    EventBus bus;
    int received = 0;

    bus.subscribe([&](const Event& event) {
        received++;
        std::cout << "EVENT................ " << EventBus::eventTypeToString(event.type) << "\\n";

        if (!event.message.empty()) {
            std::cout << "Message.............. " << event.message << "\\n";
        }

        if (!event.value.empty()) {
            std::cout << "Value................ " << event.value << "\\n";
        }

        if (event.numericValue != 0.0) {
            std::cout << "Numeric.............. " << event.numericValue << "\\n";
        }
    });

    bus.publish(Event{EventType::ExecutionStarted, 1, 42, "Execution started", "", 0.0});
    bus.publish(Event{EventType::Speed, 1, 42, "Speed sample", "MKey/s", 1334.62});
    bus.publish(Event{EventType::ExecutionFinished, 1, 42, "Execution finished", "", 0.0});

    std::cout << "Events received...... " << received << "\\n";

    return received == 3 ? 0 : 1;
}

} // namespace openpuzzle
EOF

python3 <<'PY'
from pathlib import Path

app = Path("OpenPuzzle/src/core/Application.cpp")
txt = app.read_text()

def remove_function(source: str, signature: str) -> str:
    start = source.find(signature)
    if start == -1:
        return source

    brace = source.find("{", start)
    if brace == -1:
        raise SystemExit(f"Could not find opening brace for {signature}")

    depth = 0
    i = brace
    in_string = False
    in_char = False
    escape = False
    in_line_comment = False
    in_block_comment = False

    while i < len(source):
        ch = source[i]
        nxt = source[i + 1] if i + 1 < len(source) else ""

        if in_line_comment:
            if ch == "\n":
                in_line_comment = False
            i += 1
            continue

        if in_block_comment:
            if ch == "*" and nxt == "/":
                in_block_comment = False
                i += 2
                continue
            i += 1
            continue

        if in_string:
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == '"':
                in_string = False
            i += 1
            continue

        if in_char:
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == "'":
                in_char = False
            i += 1
            continue

        if ch == "/" and nxt == "/":
            in_line_comment = True
            i += 2
            continue

        if ch == "/" and nxt == "*":
            in_block_comment = True
            i += 2
            continue

        if ch == '"':
            in_string = True
            i += 1
            continue

        if ch == "'":
            in_char = True
            i += 1
            continue

        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                end = i + 1
                while end < len(source) and source[end] in " \t\r\n":
                    end += 1
                return source[:start].rstrip() + "\n\n" + source[end:].lstrip()

        i += 1

    raise SystemExit(f"Could not find end of function {signature}")

signatures = [
    "int Application::cmdProcessTest(",
    "int Application::cmdExecutionTest(",
    "int Application::cmdSessionTest(",
    "int Application::cmdEventTest(",
]

for sig in signatures:
    txt = remove_function(txt, sig)

app.write_text(txt)

cmake = Path("OpenPuzzle/CMakeLists.txt")
ctxt = cmake.read_text()
if "src/core/commands/ApplicationTestCommands.cpp" not in ctxt:
    marker = "src/core/Application.cpp"
    if marker not in ctxt:
        raise SystemExit("Could not find src/core/Application.cpp in CMakeLists.txt")
    ctxt = ctxt.replace(marker, marker + "\n    src/core/commands/ApplicationTestCommands.cpp", 1)
    cmake.write_text(ctxt)
PY

clang-format -i OpenPuzzle/src/core/Application.cpp OpenPuzzle/src/core/commands/ApplicationTestCommands.cpp OpenPuzzle/include/openpuzzle/core/Application.hpp || true

echo "Application test commands moved to src/core/commands/ApplicationTestCommands.cpp"
echo
echo "Build and test:"
echo "  cd OpenPuzzle"
echo "  rm -rf build && mkdir build && cd build"
echo "  cmake .."
echo "  cmake --build ."
echo "  ./OpenPuzzle process-test"
echo "  ./OpenPuzzle execution-test"
echo "  ./OpenPuzzle session-test"
echo "  ./OpenPuzzle event-test"

#!/usr/bin/env bash
set -euo pipefail

if [ ! -d "OpenPuzzle" ]; then
  echo "ERROR: run this from the repo root that contains the OpenPuzzle/ directory."
  exit 1
fi

mkdir -p OpenPuzzle/include/openpuzzle/core
mkdir -p OpenPuzzle/src/core
mkdir -p OpenPuzzle/docs

cat > OpenPuzzle/include/openpuzzle/core/ExecutionManager.hpp <<'EOF'
#pragma once

#include "openpuzzle/core/ProcessRunner.hpp"
#include "openpuzzle/adapters/bitcrack/BitCrackOutputParser.hpp"

#include <string>

namespace openpuzzle {

struct ExecutionSummary {
    bool started = false;
    int exitCode = -1;

    int totalLines = 0;
    int speedEvents = 0;
    int errorEvents = 0;
    int foundEvents = 0;
    int finishedEvents = 0;

    double lastSpeedMKeys = 0.0;
};

class ExecutionManager {
public:
    ExecutionSummary runCommand(const std::string& command, bool echoOutput = true) const;
};

} // namespace openpuzzle
EOF

cat > OpenPuzzle/src/core/ExecutionManager.cpp <<'EOF'
#include "openpuzzle/core/ExecutionManager.hpp"

#include <iostream>

namespace openpuzzle {

ExecutionSummary ExecutionManager::runCommand(const std::string& command, bool echoOutput) const {
    ExecutionSummary summary;

    ProcessRunner runner;
    bitcrack::BitCrackOutputParser parser;

    auto result = runner.run(command, [&](const std::string& line) {
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

} // namespace openpuzzle
EOF

cat > OpenPuzzle/docs/EXECUTION_MANAGER.md <<'EOF'
# ExecutionManager

`ExecutionManager` is the first orchestration layer above `ProcessRunner`.

Current responsibilities:

- execute a command;
- read output line by line;
- send each line to `BitCrackOutputParser`;
- summarize parsed events.

It still does **not** update SQLite. That comes in the next step.

## Test

```bash
./OpenPuzzle execution-test
```

Expected:

```text
Started.............. yes
Exit code............ 0
Lines................ 3
Speed events......... 1
Last speed........... 1334.62 MKey/s
Error events......... 0
Found events......... 0
Finished events...... 1
```
EOF

python3 <<'PY'
from pathlib import Path

cmake = Path("OpenPuzzle/CMakeLists.txt")
txt = cmake.read_text()
if "src/core/ExecutionManager.cpp" not in txt:
    marker = "src/core/ProcessRunner.cpp"
    if marker in txt:
        txt = txt.replace(marker, marker + "\n    src/core/ExecutionManager.cpp", 1)
    else:
        marker = "src/core/Application.cpp"
        if marker not in txt:
            raise SystemExit("Could not patch CMakeLists.txt")
        txt = txt.replace(marker, marker + "\n    src/core/ExecutionManager.cpp", 1)
    cmake.write_text(txt)

hpp_path = Path("OpenPuzzle/include/openpuzzle/core/Application.hpp")
hpp = hpp_path.read_text()
if "cmdExecutionTest" not in hpp:
    marker = "int cmdProcessTest(const std::vector<std::string>&);"
    if marker in hpp:
        hpp = hpp.replace(marker, marker + " int cmdExecutionTest(const std::vector<std::string>&);", 1)
    else:
        marker = "int cmdStartJob(const std::vector<std::string>&);"
        if marker not in hpp:
            raise SystemExit("Could not find insertion point in Application.hpp")
        hpp = hpp.replace(marker, marker + " int cmdExecutionTest(const std::vector<std::string>&);", 1)
    hpp_path.write_text(hpp)

cpp_path = Path("OpenPuzzle/src/core/Application.cpp")
cpp = cpp_path.read_text()

if '#include "openpuzzle/core/ExecutionManager.hpp"' not in cpp:
    first_include = cpp.find("#include")
    end_line = cpp.find("\n", first_include)
    cpp = cpp[:end_line+1] + '#include "openpuzzle/core/ExecutionManager.hpp"\n' + cpp[end_line+1:]

if 'cmd=="execution-test"' not in cpp and 'cmd == "execution-test"' not in cpp:
    if 'if(cmd=="process-test")return cmdProcessTest(r);' in cpp:
        cpp = cpp.replace(
            'if(cmd=="process-test")return cmdProcessTest(r);',
            'if(cmd=="process-test")return cmdProcessTest(r); if(cmd=="execution-test")return cmdExecutionTest(r);',
            1
        )
    elif 'if (cmd == "process-test") return cmdProcessTest(rest);' in cpp:
        cpp = cpp.replace(
            'if (cmd == "process-test") return cmdProcessTest(rest);',
            'if (cmd == "process-test") return cmdProcessTest(rest);\n        if (cmd == "execution-test") return cmdExecutionTest(rest);',
            1
        )
    elif 'if(cmd=="start-job")return cmdStartJob(r);' in cpp:
        cpp = cpp.replace(
            'if(cmd=="start-job")return cmdStartJob(r);',
            'if(cmd=="start-job")return cmdStartJob(r); if(cmd=="execution-test")return cmdExecutionTest(r);',
            1
        )
    else:
        raise SystemExit("Could not patch command dispatcher in Application.cpp")

if "Application::cmdExecutionTest" not in cpp:
    func = r'''
int Application::cmdExecutionTest(const std::vector<std::string>& args) {
    std::string command = getArg(
        args,
        "--command",
        "printf '[Info] Starting at: 400000000000000000\\n[Info] 1334.62 MKey/s\\nFinished\\n'"
    );

    ExecutionManager manager;
    auto summary = manager.runCommand(command, true);

    std::cout << "Started.............. " << (summary.started ? "yes" : "no") << "\n";
    std::cout << "Exit code............ " << summary.exitCode << "\n";
    std::cout << "Lines................ " << summary.totalLines << "\n";
    std::cout << "Speed events......... " << summary.speedEvents << "\n";
    std::cout << "Last speed........... " << summary.lastSpeedMKeys << " MKey/s\n";
    std::cout << "Error events......... " << summary.errorEvents << "\n";
    std::cout << "Found events......... " << summary.foundEvents << "\n";
    std::cout << "Finished events...... " << summary.finishedEvents << "\n";

    return summary.started && summary.exitCode == 0 ? 0 : 1;
}

'''
    marker = "} // namespace openpuzzle"
    idx = cpp.rfind(marker)
    if idx != -1:
        cpp = cpp[:idx] + func + "\n" + cpp[idx:]
    else:
        cpp += "\nnamespace openpuzzle {\n" + func + "\n}\n"

cpp_path.write_text(cpp)

readme = Path("OpenPuzzle/README.md")
rtxt = readme.read_text()
if "ExecutionManager test" not in rtxt:
    rtxt += "\n\n## ExecutionManager test\n\n```bash\n./OpenPuzzle execution-test\n```\n"
    readme.write_text(rtxt)

changelog = Path("OpenPuzzle/CHANGELOG.md")
ctxt = changelog.read_text()
if "0.15-dev — ExecutionManager" not in ctxt:
    ctxt += "\n\n## 0.15-dev — ExecutionManager\n\n- Added `ExecutionManager`.\n- Added `execution-test` command.\n- Added `docs/EXECUTION_MANAGER.md`.\n- ExecutionManager runs commands through ProcessRunner and parses BitCrack-style output.\n"
    changelog.write_text(ctxt)
PY

echo "ExecutionManager v1 applied."
echo
echo "Build and test:"
echo "  cd OpenPuzzle"
echo "  rm -rf build && mkdir build && cd build"
echo "  cmake .."
echo "  cmake --build ."
echo "  ./OpenPuzzle execution-test"

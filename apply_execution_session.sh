#!/usr/bin/env bash
set -euo pipefail

if [ ! -d "OpenPuzzle" ]; then
  echo "ERROR: run this from the repo root that contains the OpenPuzzle/ directory."
  exit 1
fi

mkdir -p OpenPuzzle/include/openpuzzle/core
mkdir -p OpenPuzzle/src/core
mkdir -p OpenPuzzle/docs

cat > OpenPuzzle/include/openpuzzle/core/ExecutionSession.hpp <<'EOF'
#pragma once

#include <string>

namespace openpuzzle {

enum class ExecutionSessionStatus {
    Created,
    Running,
    Finished,
    Failed,
    Cancelled
};

struct ExecutionSession {
    int executionId = 0;
    int jobId = 0;
    int puzzleId = 0;
    int rangeId = 0;

    std::string engine;
    std::string workspace;
    std::string command;

    ExecutionSessionStatus status = ExecutionSessionStatus::Created;

    static std::string statusToString(ExecutionSessionStatus status);
};

} // namespace openpuzzle
EOF

cat > OpenPuzzle/src/core/ExecutionSession.cpp <<'EOF'
#include "openpuzzle/core/ExecutionSession.hpp"

namespace openpuzzle {

std::string ExecutionSession::statusToString(ExecutionSessionStatus status) {
    switch (status) {
        case ExecutionSessionStatus::Created:
            return "CREATED";
        case ExecutionSessionStatus::Running:
            return "RUNNING";
        case ExecutionSessionStatus::Finished:
            return "FINISHED";
        case ExecutionSessionStatus::Failed:
            return "FAILED";
        case ExecutionSessionStatus::Cancelled:
            return "CANCELLED";
    }

    return "UNKNOWN";
}

} // namespace openpuzzle
EOF

cat > OpenPuzzle/docs/EXECUTION_SESSION.md <<'EOF'
# ExecutionSession

`ExecutionSession` represents one attempt to execute a job.

A single `Job` may have multiple execution sessions.

Example:

```text
Job 42
  Execution 1 -> interrupted
  Execution 2 -> resumed
  Execution 3 -> finished
```

This separation is important for recovery, auditing and future dashboard replay.

## Current fields

- `executionId`
- `jobId`
- `puzzleId`
- `rangeId`
- `engine`
- `workspace`
- `command`
- `status`

## Status values

- `CREATED`
- `RUNNING`
- `FINISHED`
- `FAILED`
- `CANCELLED`
EOF

python3 <<'PY'
from pathlib import Path

cmake = Path("OpenPuzzle/CMakeLists.txt")
txt = cmake.read_text()
if "src/core/ExecutionSession.cpp" not in txt:
    marker = "src/core/ExecutionManager.cpp"
    if marker in txt:
        txt = txt.replace(marker, marker + "\n    src/core/ExecutionSession.cpp", 1)
    else:
        marker = "src/core/Application.cpp"
        if marker not in txt:
            raise SystemExit("Could not patch CMakeLists.txt")
        txt = txt.replace(marker, marker + "\n    src/core/ExecutionSession.cpp", 1)
    cmake.write_text(txt)

hpp_path = Path("OpenPuzzle/include/openpuzzle/core/Application.hpp")
hpp = hpp_path.read_text()
if "cmdSessionTest" not in hpp:
    marker = "int cmdExecutionTest(const std::vector<std::string>&);"
    if marker in hpp:
        hpp = hpp.replace(marker, marker + " int cmdSessionTest(const std::vector<std::string>&);", 1)
    else:
        marker = "int cmdProcessTest(const std::vector<std::string>&);"
        if marker not in hpp:
            raise SystemExit("Could not find insertion point in Application.hpp")
        hpp = hpp.replace(marker, marker + " int cmdSessionTest(const std::vector<std::string>&);", 1)
    hpp_path.write_text(hpp)

cpp_path = Path("OpenPuzzle/src/core/Application.cpp")
cpp = cpp_path.read_text()

if '#include "openpuzzle/core/ExecutionSession.hpp"' not in cpp:
    first_include = cpp.find("#include")
    end_line = cpp.find("\n", first_include)
    cpp = cpp[:end_line+1] + '#include "openpuzzle/core/ExecutionSession.hpp"\n' + cpp[end_line+1:]

if 'cmd=="session-test"' not in cpp and 'cmd == "session-test"' not in cpp:
    if 'if(cmd=="execution-test")return cmdExecutionTest(r);' in cpp:
        cpp = cpp.replace(
            'if(cmd=="execution-test")return cmdExecutionTest(r);',
            'if(cmd=="execution-test")return cmdExecutionTest(r); if(cmd=="session-test")return cmdSessionTest(r);',
            1
        )
    elif 'if(cmd=="process-test")return cmdProcessTest(r);' in cpp:
        cpp = cpp.replace(
            'if(cmd=="process-test")return cmdProcessTest(r);',
            'if(cmd=="process-test")return cmdProcessTest(r); if(cmd=="session-test")return cmdSessionTest(r);',
            1
        )
    else:
        raise SystemExit("Could not patch command dispatcher in Application.cpp")

if "Application::cmdSessionTest" not in cpp:
    func = r'''
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

    std::cout << "Execution ID......... " << session.executionId << "\n";
    std::cout << "Job ID............... " << session.jobId << "\n";
    std::cout << "Puzzle ID............ " << session.puzzleId << "\n";
    std::cout << "Range ID............. " << session.rangeId << "\n";
    std::cout << "Engine............... " << session.engine << "\n";
    std::cout << "Workspace............ " << session.workspace << "\n";
    std::cout << "Status............... " << ExecutionSession::statusToString(session.status) << "\n";

    return 0;
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
if "ExecutionSession test" not in rtxt:
    rtxt += "\n\n## ExecutionSession test\n\n```bash\n./OpenPuzzle session-test\n```\n"
    readme.write_text(rtxt)

changelog = Path("OpenPuzzle/CHANGELOG.md")
ctxt = changelog.read_text()
if "0.16-dev — ExecutionSession" not in ctxt:
    ctxt += "\n\n## 0.16-dev — ExecutionSession\n\n- Added `ExecutionSession` model.\n- Added `session-test` command.\n- Added `docs/EXECUTION_SESSION.md`.\n- Prepared the execution lifecycle model for recovery and replay.\n"
    changelog.write_text(ctxt)
PY

echo "ExecutionSession applied."
echo
echo "Build and test:"
echo "  cd OpenPuzzle"
echo "  rm -rf build && mkdir build && cd build"
echo "  cmake .."
echo "  cmake --build ."
echo "  ./OpenPuzzle session-test"

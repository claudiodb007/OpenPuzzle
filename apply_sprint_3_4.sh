#!/usr/bin/env bash
set -e

ROOT="OpenPuzzle"

echo "===================================="
echo " Applying Sprint 3.4"
echo "===================================="

python3 <<'PY'
from pathlib import Path

hpp = Path("OpenPuzzle/include/openpuzzle/core/RecoveryManager.hpp")
txt = hpp.read_text()

if "buildExecutionContext" not in txt:
    txt = txt.replace(
        "#include \"openpuzzle/core/RecoveryState.hpp\"",
        "#include \"openpuzzle/core/ExecutionContext.hpp\"\n#include \"openpuzzle/core/RecoveryState.hpp\""
    )

    txt = txt.replace(
        "RecoveryState load(int jobId) const;",
        "RecoveryState load(int jobId) const;\n\n    ExecutionContext buildExecutionContext(int jobId) const;"
    )

hpp.write_text(txt)
print("RecoveryManager.hpp atualizado")
PY

python3 <<'PY'
from pathlib import Path

cpp = Path("OpenPuzzle/src/core/RecoveryManager.cpp")
txt = cpp.read_text()

if "buildExecutionContext" not in txt:

    txt = txt.replace(
        "} // namespace openpuzzle",
r'''

ExecutionContext RecoveryManager::buildExecutionContext(int jobId) const
{
    ExecutionContext ctx;

    ctx.jobId = jobId;
    ctx.workspace = workspaceManager_.jobWorkspace(jobId).string();

    return ctx;
}

} // namespace openpuzzle'''
    )

cpp.write_text(txt)
print("RecoveryManager.cpp atualizado")
PY

mkdir -p OpenPuzzle/tests/core

cat > OpenPuzzle/tests/core/ExecutionContextRecoveryTests.cpp <<'CPP'
#include "openpuzzle/core/RecoveryManager.hpp"
#include "openpuzzle/core/WorkspaceManager.hpp"

#include <filesystem>

using namespace openpuzzle;

int main()
{
    auto temp =
        std::filesystem::temp_directory_path() /
        "openpuzzle_context_recovery";

    std::filesystem::remove_all(temp);

    WorkspaceManager workspace(temp);

    workspace.createJobWorkspace(42);

    RecoveryManager recovery(workspace);

    auto ctx = recovery.buildExecutionContext(42);

    if(ctx.jobId != 42)
        return 1;

    if(ctx.workspace.empty())
        return 2;

    if(ctx.workspace.find("00000042") == std::string::npos)
        return 3;

    std::filesystem::remove_all(temp);

    return 0;
}
CPP

python3 <<'PY'
from pathlib import Path

cm = Path("OpenPuzzle/tests/CMakeLists.txt")
txt = cm.read_text()

if "ExecutionContextRecoveryTests" not in txt:
    txt += '''

add_executable(ExecutionContextRecoveryTests
    core/ExecutionContextRecoveryTests.cpp
)

target_link_libraries(ExecutionContextRecoveryTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ExecutionContextRecoveryTests
    COMMAND ExecutionContextRecoveryTests
)
'''

cm.write_text(txt)
print("tests/CMakeLists atualizado")
PY

echo
echo "Compilar..."

cd OpenPuzzle
./scripts/format.sh
./scripts/test.sh

echo
echo "===================================="
echo " Sprint 3.4 aplicado"
echo "===================================="

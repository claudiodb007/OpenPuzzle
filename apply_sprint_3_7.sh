#!/usr/bin/env bash
set -euo pipefail

python3 <<'PY'
from pathlib import Path

p = Path("OpenPuzzle/src/core/RecoveryManager.cpp")
txt = p.read_text()

if "extractBool" not in txt:
    marker = "static double extractDouble"
    txt = txt.replace(marker, r'''static bool extractBool(const std::string& text, const std::string& key, bool defaultValue)
{
    auto pos = text.find(key);

    if (pos == std::string::npos) return defaultValue;

    pos = text.find(':', pos);

    if (pos == std::string::npos) return defaultValue;

    auto value = text.substr(pos + 1);

    if (value.find("true") != std::string::npos) return true;
    if (value.find("false") != std::string::npos) return false;

    return defaultValue;
}

static double extractDouble''')

old = '''    ctx.engine = extractString(content, "\\"engine\\"", "");
    ctx.command = extractString(content, "\\"command\\"", "");
    ctx.workspace = workspaceManager_.jobWorkspace(ctx.jobId).string();

    return ctx;'''

new = '''    ctx.engine = extractString(content, "\\"engine\\"", "");
    ctx.command = extractString(content, "\\"command\\"", "");
    ctx.workspace = extractString(content, "\\"workspace\\"", workspaceManager_.jobWorkspace(ctx.jobId).string());
    ctx.echoOutput = extractBool(content, "\\"echo_output\\"", true);

    return ctx;'''

if old not in txt:
    raise SystemExit("Bloco esperado não encontrado em RecoveryManager.cpp")

txt = txt.replace(old, new, 1)
p.write_text(txt)
PY

cat > OpenPuzzle/tests/core/ExecutionContextRecoveryTests.cpp <<'EOF2'
#include "openpuzzle/core/RecoveryManager.hpp"
#include "openpuzzle/core/WorkspaceManager.hpp"

#include <filesystem>
#include <fstream>

using namespace openpuzzle;

int main()
{
    auto temp =
        std::filesystem::temp_directory_path() /
        "openpuzzle_context_recovery";

    std::filesystem::remove_all(temp);

    WorkspaceManager workspace(temp);
    workspace.createJobWorkspace(42);

    auto workspacePath = workspace.jobWorkspace(42).string();

    {
        std::ofstream execution(workspace.executionFile(42));
        execution << "{\n";
        execution << "  \"execution_id\": 7,\n";
        execution << "  \"puzzle_id\": 71,\n";
        execution << "  \"job_id\": 42,\n";
        execution << "  \"range_id\": 1001,\n";
        execution << "  \"engine\": \"BitCrack\",\n";
        execution << "  \"command\": \"printf test\",\n";
        execution << "  \"workspace\": \"" << workspacePath << "\",\n";
        execution << "  \"echo_output\": false\n";
        execution << "}\n";
    }

    RecoveryManager recovery(workspace);
    auto ctx = recovery.buildExecutionContext(42);

    if (ctx.executionId != 7) return 1;
    if (ctx.puzzleId != 71) return 2;
    if (ctx.jobId != 42) return 3;
    if (ctx.rangeId != 1001) return 4;
    if (ctx.engine != "BitCrack") return 5;
    if (ctx.command != "printf test") return 6;
    if (ctx.workspace.find("00000042") == std::string::npos) return 7;
    if (ctx.echoOutput != false) return 8;

    std::filesystem::remove_all(temp);

    return 0;
}
EOF2

cd OpenPuzzle
./scripts/format.sh
./scripts/test.sh

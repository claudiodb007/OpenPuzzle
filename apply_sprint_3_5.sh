#!/usr/bin/env bash
set -euo pipefail

echo "===================================="
echo " Applying Sprint 3.5"
echo "===================================="

python3 <<'PY'
from pathlib import Path

p = Path("OpenPuzzle/src/core/RecoveryManager.cpp")
txt = p.read_text()

if "extractString" not in txt:
    txt = txt.replace(
        "static double extractDouble",
        r'''static std::string extractString(const std::string& text, const std::string& key, const std::string& defaultValue)
{
    auto pos = text.find(key);

    if (pos == std::string::npos) {
        return defaultValue;
    }

    pos = text.find(':', pos);

    if (pos == std::string::npos) {
        return defaultValue;
    }

    auto firstQuote = text.find('"', pos + 1);

    if (firstQuote == std::string::npos) {
        return defaultValue;
    }

    auto secondQuote = text.find('"', firstQuote + 1);

    if (secondQuote == std::string::npos) {
        return defaultValue;
    }

    return text.substr(firstQuote + 1, secondQuote - firstQuote - 1);
}

static double extractDouble'''
    )

old = r'''ExecutionContext RecoveryManager::buildExecutionContext(int jobId) const
{
    ExecutionContext ctx;

    ctx.jobId = jobId;
    ctx.workspace = workspaceManager_.jobWorkspace(jobId).string();

    return ctx;
}'''

new = r'''ExecutionContext RecoveryManager::buildExecutionContext(int jobId) const
{
    ExecutionContext ctx;

    ctx.jobId = jobId;
    ctx.workspace = workspaceManager_.jobWorkspace(jobId).string();

    auto content = readFile(workspaceManager_.executionFile(jobId));

    if (content.empty()) {
        return ctx;
    }

    ctx.executionId = extractInt(content, "\"execution_id\"", 0);
    ctx.puzzleId = extractInt(content, "\"puzzle_id\"", 0);
    ctx.jobId = extractInt(content, "\"job_id\"", jobId);
    ctx.rangeId = extractInt(content, "\"range_id\"", 0);
    ctx.engine = extractString(content, "\"engine\"", "");
    ctx.command = extractString(content, "\"command\"", "");
    ctx.workspace = workspaceManager_.jobWorkspace(ctx.jobId).string();

    return ctx;
}'''

if old not in txt:
    raise SystemExit("Não encontrei buildExecutionContext antigo")

txt = txt.replace(old, new, 1)
p.write_text(txt)
PY

cat > OpenPuzzle/tests/core/ExecutionContextRecoveryTests.cpp <<'CPP'
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

    {
        std::ofstream execution(workspace.executionFile(42));
        execution << "{\n";
        execution << "  \"execution_id\": 7,\n";
        execution << "  \"puzzle_id\": 71,\n";
        execution << "  \"job_id\": 42,\n";
        execution << "  \"range_id\": 1001,\n";
        execution << "  \"engine\": \"BitCrack\",\n";
        execution << "  \"command\": \"printf test\"\n";
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

    std::filesystem::remove_all(temp);

    return 0;
}
CPP

cd OpenPuzzle
./scripts/format.sh
./scripts/test.sh

echo "===================================="
echo " Sprint 3.5 aplicado"
echo "===================================="

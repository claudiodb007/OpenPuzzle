#!/usr/bin/env bash
set -euo pipefail

python3 - <<'PY'
from pathlib import Path

p = Path("OpenPuzzle/src/core/RecoveryManager.cpp")
txt = p.read_text()

start = txt.find("static std::string extractString")
if start == -1:
    raise SystemExit("Não encontrei extractString")

brace = txt.find("{", start)
depth = 0
end = None

for i in range(brace, len(txt)):
    if txt[i] == "{":
        depth += 1
    elif txt[i] == "}":
        depth -= 1
        if depth == 0:
            end = i + 1
            break

if end is None:
    raise SystemExit("Não encontrei fim de extractString")

new_func = r'''static std::string extractString(const std::string& text, const std::string& key, const std::string& defaultValue)
{
    auto pos = text.find(key);

    if (pos == std::string::npos) {
        return defaultValue;
    }

    pos = text.find(':', pos);

    if (pos == std::string::npos) {
        return defaultValue;
    }

    auto quote = text.find('"', pos + 1);

    if (quote == std::string::npos) {
        return defaultValue;
    }

    std::string value;
    bool escaped = false;

    for (std::size_t i = quote + 1; i < text.size(); ++i) {
        char c = text[i];

        if (escaped) {
            switch (c) {
                case 'n':
                    value.push_back('\n');
                    break;
                case 't':
                    value.push_back('\t');
                    break;
                case '"':
                    value.push_back('"');
                    break;
                case '\\':
                    value.push_back('\\');
                    break;
                default:
                    value.push_back(c);
                    break;
            }

            escaped = false;
            continue;
        }

        if (c == '\\') {
            escaped = true;
            continue;
        }

        if (c == '"') {
            return value;
        }

        value.push_back(c);
    }

    return defaultValue;
}'''

txt = txt[:start] + new_func + txt[end:]
p.write_text(txt)
print("RecoveryManager extractString atualizado")
PY

cat > OpenPuzzle/tests/core/ExecutionContextEscapedCommandTests.cpp <<'EOF2'
#include "openpuzzle/core/RecoveryManager.hpp"
#include "openpuzzle/core/WorkspaceManager.hpp"

#include <filesystem>
#include <fstream>

using namespace openpuzzle;

int main()
{
    auto temp =
        std::filesystem::temp_directory_path() /
        "openpuzzle_escaped_command_recovery";

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
        execution << "  \"command\": \"printf \\\"hello\\\"\",\n";
        execution << "  \"workspace\": \"" << workspace.jobWorkspace(42).string() << "\",\n";
        execution << "  \"echo_output\": true\n";
        execution << "}\n";
    }

    RecoveryManager recovery(workspace);
    auto ctx = recovery.buildExecutionContext(42);

    if (ctx.command != "printf \"hello\"") return 1;

    std::filesystem::remove_all(temp);

    return 0;
}
EOF2

python3 - <<'PY'
from pathlib import Path

p = Path("OpenPuzzle/tests/CMakeLists.txt")
txt = p.read_text()

if "ExecutionContextEscapedCommandTests" not in txt:
    txt += """

add_executable(ExecutionContextEscapedCommandTests
    core/ExecutionContextEscapedCommandTests.cpp
)

target_link_libraries(ExecutionContextEscapedCommandTests
    PRIVATE
        OpenPuzzleCore
)

add_test(
    NAME ExecutionContextEscapedCommandTests
    COMMAND ExecutionContextEscapedCommandTests
)
"""

p.write_text(txt)
print("Teste adicionado")
PY

cd OpenPuzzle
./scripts/format.sh
./scripts/test.sh

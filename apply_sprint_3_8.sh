#!/usr/bin/env bash
set -euo pipefail

python3 <<'PY'
from pathlib import Path

p = Path("OpenPuzzle/src/core/commands/ApplicationTestCommands.cpp")
txt = p.read_text()

old = """    std::cout << "Workspace....... " << workspace.jobWorkspace(jobId) << "\\n";

    return 0;
}"""

new = """    std::cout << "Workspace....... " << workspace.jobWorkspace(jobId) << "\\n";

    if (hasArg(args, "--run")) {
        std::cout << "\\n";
        std::cout << "Rebuilding execution context...\\n";

        auto ctx = recovery.buildExecutionContext(jobId);

        ExecutionManager manager;

        auto result = manager.run(ctx);

        std::cout << "\\nExecution finished\\n";
        std::cout << "Exit code....... " << result.exitCode << "\\n";
        std::cout << "Lines........... " << result.linesRead << "\\n";
        std::cout << "Average speed... " << result.averageSpeed << " MKey/s\\n";
    }

    return 0;
}"""

if old not in txt:
    raise SystemExit("Não encontrei cmdResume")

txt = txt.replace(old, new, 1)

p.write_text(txt)
print("cmdResume atualizado")
PY

cd OpenPuzzle

./scripts/format.sh
./scripts/test.sh

echo
echo "======================================="
echo "Sprint 3.8 aplicado"
echo "======================================="

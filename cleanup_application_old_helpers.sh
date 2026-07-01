#!/usr/bin/env bash
set -euo pipefail

python3 - <<'PY'
from pathlib import Path

p = Path("OpenPuzzle/src/core/Application.cpp")
txt = p.read_text()

def remove_function(source, name):
    start = source.find(f"static std::string {name}")
    if start == -1:
        return source

    brace = source.find("{", start)
    depth = 0
    end = None

    for i in range(brace, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                end = i + 1
                break

    if end is None:
        raise SystemExit(f"Não encontrei fim da função {name}")

    return source[:start].rstrip() + "\n\n" + source[end:].lstrip()

txt = remove_function(txt, "wsFor")
txt = remove_function(txt, "bcCmd")

p.write_text(txt)
print("Helpers antigos removidos da Application.cpp")
PY

cd OpenPuzzle
./scripts/format.sh
./scripts/test.sh

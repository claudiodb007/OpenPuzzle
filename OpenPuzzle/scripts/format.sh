#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cd "$ROOT_DIR"

find include src tests -name "*.hpp" -o -name "*.cpp" | xargs clang-format -i

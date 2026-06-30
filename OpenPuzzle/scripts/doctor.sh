#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cd "$ROOT_DIR"

echo "======================================"
echo "        OpenPuzzle Doctor"
echo "======================================"
echo

echo -n "CMake................ "
command -v cmake >/dev/null && echo "OK" || echo "MISSING"

echo -n "C++ compiler......... "
command -v c++ >/dev/null && echo "OK" || echo "MISSING"

echo -n "clang-format......... "
command -v clang-format >/dev/null && echo "OK" || echo "MISSING"

echo -n "SQLite3 library...... "
pkg-config --exists sqlite3 && echo "OK" || echo "CHECK MANUALLY"

echo -n "Build directory...... "
[ -d build ] && echo "OK" || echo "MISSING"

echo -n "OpenPuzzle binary.... "
[ -x build/OpenPuzzle ] && echo "OK" || echo "MISSING"

echo
echo "Doctor finished."

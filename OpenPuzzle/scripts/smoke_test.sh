#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build"

rm -rf "$BUILD"
mkdir -p "$BUILD"
cd "$BUILD"

cmake ..
cmake --build .

TEST_HOME="$(mktemp -d)"
export HOME="$TEST_HOME"

./OpenPuzzle init
./OpenPuzzle import-puzzle-json --file ../resources/puzzles/71.json
./OpenPuzzle list-puzzles
./OpenPuzzle create-job --puzzle 71 --block-bits 40
./OpenPuzzle list-ranges --puzzle 71
./OpenPuzzle dashboard --puzzle 71

echo
echo "Smoke test OK"

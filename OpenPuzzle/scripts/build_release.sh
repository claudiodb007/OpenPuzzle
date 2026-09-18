#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="$(sed -nE 's/^project\(OpenPuzzle VERSION ([0-9]+\.[0-9]+\.[0-9]+) LANGUAGES CXX\)$/\1/p' "$ROOT/CMakeLists.txt")"
DIST="${1:-$ROOT/dist/release-$VERSION}"

[[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || {
  echo "Unable to determine the OpenPuzzle version" >&2
  exit 1
}

for command in cmake ctest cpack sha256sum; do
  command -v "$command" >/dev/null 2>&1 || {
    echo "Required command is missing: $command" >&2
    exit 1
  }
done

mkdir -p "$DIST"
DIST="$(cd "$DIST" && pwd)"
if [[ -n "$(find "$DIST" -mindepth 1 -maxdepth 1 -print -quit)" ]]; then
  echo "Release output directory must be empty: $DIST" >&2
  exit 1
fi

BUILD_DIR="$(mktemp -d -t openpuzzle-release-build.XXXXXXXX)"
trap 'rm -rf -- "$BUILD_DIR"' EXIT
JOBS="$(nproc 2>/dev/null || echo 1)"
export OPENPUZZLE_RELEASE_VERSION="$VERSION"

echo "===== CONFIGURE ====="
cmake \
  -S "$ROOT" \
  -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DOPENPUZZLE_BUILD_UI=ON

echo
echo "===== BUILD ====="
cmake --build "$BUILD_DIR" --parallel "$JOBS"

echo
echo "===== REQUIRED DESKTOP UI ====="
cmake --build "$BUILD_DIR" --target OpenPuzzleUi --parallel "$JOBS"

echo
echo "===== TESTS ====="
QT_QPA_PLATFORM=offscreen \
ctest \
  --test-dir "$BUILD_DIR" \
  --output-on-failure \
  --parallel "$JOBS"

echo
echo "===== CPACK ====="
cpack \
  --config "$BUILD_DIR/CPackConfig.cmake" \
  -B "$DIST"

echo
echo "===== UPDATER ARTIFACTS ====="
"$ROOT/scripts/create_release_manifests.sh" "$DIST" "$VERSION"

echo
echo "===== FINAL VALIDATION ====="
"$ROOT/scripts/validate_release_artifacts.sh" "$DIST" "$VERSION"

echo
echo "===== RELEASE OUTPUT ====="
find "$DIST" -maxdepth 1 -type f -printf '%f\n' | sort
echo
echo "Release build completed: $DIST"

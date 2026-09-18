#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="$(sed -nE 's/^project\(OpenPuzzle VERSION ([0-9]+\.[0-9]+\.[0-9]+) LANGUAGES CXX\)$/\1/p' "$ROOT/CMakeLists.txt")"
DIST="${1:-$ROOT/dist/release-$VERSION}"

[[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || {
  echo "Unable to determine the OpenPuzzle version" >&2
  exit 1
}

for command in cmake ctest cpack git sha256sum; do
  command -v "$command" >/dev/null 2>&1 || {
    echo "Required command is missing: $command" >&2
    exit 1
  }
done

REPOSITORY="$(git -C "$ROOT" rev-parse --show-toplevel 2>/dev/null)" || {
  echo "OpenPuzzle source is not inside a Git repository" >&2
  exit 1
}
[[ -z "$(git -C "$REPOSITORY" status --porcelain --untracked-files=all)" ]] || {
  echo "Release builds require a clean Git worktree" >&2
  exit 1
}
git -C "$REPOSITORY" diff --check

COMMIT="$(git -C "$REPOSITORY" rev-parse HEAD)"
BRANCH="$(git -C "$REPOSITORY" branch --show-current)"
[[ -n "$BRANCH" ]] || BRANCH="detached"
COMMIT_DATE="$(git -C "$REPOSITORY" show -s --format=%cI "$COMMIT")"
SOURCE_FILES="$(git -C "$REPOSITORY" ls-tree -r --name-only "$COMMIT" | wc -l)"

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
echo "===== PORTABLE PACKAGE ====="
"$ROOT/scripts/create_release_manifests.sh" "$DIST" "$VERSION"

echo
echo "===== SOURCE ARCHIVE ====="
SOURCE_ARCHIVE="$DIST/OpenPuzzle-$VERSION-source.tar.gz"
git -C "$REPOSITORY" archive \
  --format=tar.gz \
  --prefix="OpenPuzzle-$VERSION-source/" \
  -o "$SOURCE_ARCHIVE" \
  "$COMMIT"

TESTS="$(ctest --test-dir "$BUILD_DIR" -N | sed -nE 's/^Total Tests: ([0-9]+)$/\1/p')"
[[ "$TESTS" =~ ^[1-9][0-9]*$ ]] || {
  echo "Unable to determine the automated test count" >&2
  exit 1
}

CUDA_ENGINE="$BUILD_DIR/libexec/OpenPuzzle/cuBitCrack"
OPENCL_ENGINE="$BUILD_DIR/libexec/OpenPuzzle/clBitCrack"
KEYHUNT_ENGINE="$BUILD_DIR/libexec/OpenPuzzle/keyhunt"
for engine in "$CUDA_ENGINE" "$OPENCL_ENGINE" "$KEYHUNT_ENGINE"; do
  [[ -f "$engine" && ! -L "$engine" ]] || {
    echo "Packaged engine is missing or unsafe: $engine" >&2
    exit 1
  }
done

DEB="$DIST/OpenPuzzle-$VERSION-Linux-x86_64.deb"
DEB_SHA="$(sha256sum "$DEB" | awk '{print $1}')"
PORTABLE="OpenPuzzle-$VERSION-portable-${DEB_SHA:0:8}.deb"
RELEASE_MANIFEST="$DIST/RELEASE_MANIFEST.txt"

cat > "$RELEASE_MANIFEST" <<EOF
OpenPuzzle release: $VERSION
Branch: $BRANCH
Commit: $COMMIT
Commit date: $COMMIT_DATE
Source tracked files: $SOURCE_FILES
Automated tests: $TESTS
Debian package: OpenPuzzle-$VERSION-Linux-x86_64.deb
Binary archive: OpenPuzzle-$VERSION-Linux-x86_64.tar.gz
Portable updater package: $PORTABLE
Source archive: OpenPuzzle-$VERSION-source.tar.gz
CUDA engine SHA-256: $(sha256sum "$CUDA_ENGINE" | awk '{print $1}')
OpenCL engine SHA-256: $(sha256sum "$OPENCL_ENGINE" | awk '{print $1}')
KeyHunt engine SHA-256: $(sha256sum "$KEYHUNT_ENGINE" | awk '{print $1}')
Tag: not created
Remote push: not performed
EOF

echo
echo "===== FINAL CHECKSUM MANIFESTS ====="
"$ROOT/scripts/create_release_manifests.sh" "$DIST" "$VERSION"

echo
echo "===== FINAL VALIDATION ====="
"$ROOT/scripts/validate_release_artifacts.sh" \
  "$DIST" "$VERSION" "$REPOSITORY" "$COMMIT"

echo
echo "===== RELEASE OUTPUT ====="
find "$DIST" -maxdepth 1 -type f -printf '%f\n' | sort
echo
echo "Release build completed: $DIST"

#!/usr/bin/env bash
set -euo pipefail

BUILD_SCRIPT="${1:?build_release.sh path is required}"
VALIDATOR="$(dirname "$BUILD_SCRIPT")/validate_release_artifacts.sh"
VERSION="${2:-}"
if [[ -z "$VERSION" ]]; then
  VERSION="$(
    sed -nE \
      's/^project\(OpenPuzzle VERSION ([0-9]+\.[0-9]+\.[0-9]+) LANGUAGES CXX\)$/\1/p' \
      "$(dirname "$BUILD_SCRIPT")/../CMakeLists.txt"
  )"
fi
[[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || {
  echo "BuildReleaseScriptTests: invalid version" >&2
  exit 1
}
TEST_ROOT="$(mktemp -d -t openpuzzle-build-release-test.XXXXXXXX)"
trap 'rm -rf -- "$TEST_ROOT"' EXIT
FAKE_BIN="$TEST_ROOT/bin"
OUTPUT="$TEST_ROOT/output"
LOG="$TEST_ROOT/commands.log"
mkdir -p "$FAKE_BIN" "$OUTPUT"

fail() {
  echo "BuildReleaseScriptTests: $*" >&2
  exit 1
}

cat > "$FAKE_BIN/cmake" <<'SCRIPT'
#!/usr/bin/env bash
set -euo pipefail
printf 'cmake %s\n' "$*" >> "$OPENPUZZLE_TEST_COMMAND_LOG"
if [[ " ${*} " == *" -B "* && " ${*} " == *" -S "* ]]; then
  arguments=("$@")
  for ((index = 0; index < ${#arguments[@]}; index++)); do
    if [[ "${arguments[index]}" = "-B" ]]; then
      mkdir -p "${arguments[index + 1]}"
      : > "${arguments[index + 1]}/CPackConfig.cmake"
    fi
  done
fi
SCRIPT

cat > "$FAKE_BIN/ctest" <<'SCRIPT'
#!/usr/bin/env bash
set -euo pipefail
printf 'ctest %s\n' "$*" >> "$OPENPUZZLE_TEST_COMMAND_LOG"
SCRIPT

cat > "$FAKE_BIN/cpack" <<'SCRIPT'
#!/usr/bin/env bash
set -euo pipefail
printf 'cpack %s\n' "$*" >> "$OPENPUZZLE_TEST_COMMAND_LOG"
output=""
while [[ "$#" -gt 0 ]]; do
  if [[ "$1" = "-B" ]]; then
    output="$2"
    shift 2
  else
    shift
  fi
done
[[ -n "$output" ]]
printf 'release DEB\n' > \
  "$output/OpenPuzzle-$OPENPUZZLE_RELEASE_VERSION-Linux-x86_64.deb"
printf 'release TGZ\n' > \
  "$output/OpenPuzzle-$OPENPUZZLE_RELEASE_VERSION-Linux-x86_64.tar.gz"
SCRIPT

chmod +x "$FAKE_BIN/cmake" "$FAKE_BIN/ctest" "$FAKE_BIN/cpack"

OPENPUZZLE_TEST_COMMAND_LOG="$LOG" \
PATH="$FAKE_BIN:$PATH" \
  "$BUILD_SCRIPT" "$OUTPUT" >/dev/null

DEB="$OUTPUT/OpenPuzzle-$VERSION-Linux-x86_64.deb"
DEB_SHA="$(sha256sum "$DEB" | awk '{print $1}')"
PORTABLE="$OUTPUT/OpenPuzzle-$VERSION-portable-${DEB_SHA:0:8}.deb"

for file in \
  "$DEB" \
  "$OUTPUT/OpenPuzzle-$VERSION-Linux-x86_64.tar.gz" \
  "$PORTABLE" \
  "$OUTPUT/OpenPuzzle-$VERSION-SHA256SUMS.txt" \
  "$OUTPUT/SHA256SUMS.txt" \
  "$OUTPUT/SHA256SUMS"; do
  [[ -f "$file" ]] || fail "missing output: $(basename "$file")"
done

cmp -s "$DEB" "$PORTABLE" || fail "portable DEB differs"
grep -Fq -- '--target OpenPuzzleUi' "$LOG" || \
  fail "desktop UI was not required"
grep -q '^ctest ' "$LOG" || fail "test suite was not invoked"
grep -q '^cpack ' "$LOG" || fail "CPack was not invoked"

if OPENPUZZLE_TEST_COMMAND_LOG="$LOG" PATH="$FAKE_BIN:$PATH" \
  "$BUILD_SCRIPT" "$OUTPUT" >/dev/null 2>&1; then
  fail "non-empty output directory was accepted"
fi

printf 'corrupt manifest\n' > "$OUTPUT/SHA256SUMS"
if "$VALIDATOR" "$OUTPUT" "$VERSION" >/dev/null 2>&1; then
  fail "corrupt conventional manifest was accepted"
fi

echo "BuildReleaseScriptTests passed"

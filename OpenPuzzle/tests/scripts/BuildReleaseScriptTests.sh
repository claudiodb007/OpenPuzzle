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
      mkdir -p "${arguments[index + 1]}/libexec/OpenPuzzle"
      printf 'CUDA engine\n' > \
        "${arguments[index + 1]}/libexec/OpenPuzzle/cuBitCrack"
      printf 'OpenCL engine\n' > \
        "${arguments[index + 1]}/libexec/OpenPuzzle/clBitCrack"
      printf 'KeyHunt engine\n' > \
        "${arguments[index + 1]}/libexec/OpenPuzzle/keyhunt"
    fi
  done
fi
SCRIPT

cat > "$FAKE_BIN/ctest" <<'SCRIPT'
#!/usr/bin/env bash
set -euo pipefail
printf 'ctest %s\n' "$*" >> "$OPENPUZZLE_TEST_COMMAND_LOG"
if [[ " ${*} " == *" -N "* ]]; then
  printf 'Total Tests: 116\n'
fi
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

cat > "$FAKE_BIN/git" <<'SCRIPT'
#!/usr/bin/env bash
set -euo pipefail
printf 'git %s\n' "$*" >> "$OPENPUZZLE_TEST_COMMAND_LOG"
if [[ "${1:-}" = "-C" ]]; then
  shift 2
fi
case "${1:-}" in
  rev-parse)
    if [[ "${2:-}" = "--show-toplevel" ]]; then
      printf '%s\n' "$OPENPUZZLE_TEST_REPOSITORY"
    elif [[ "${2:-}" = "--is-inside-work-tree" ]]; then
      printf 'true\n'
    else
      printf '%040d\n' 0
    fi
    ;;
  status)
    if [[ "${OPENPUZZLE_TEST_DIRTY:-0}" = "1" ]]; then
      printf ' M OpenPuzzle/CMakeLists.txt\n'
    fi
    ;;
  diff)
    ;;
  branch)
    printf 'feature/1.0.26-release-contract\n'
    ;;
  show)
    printf '2026-09-18T12:00:00+00:00\n'
    ;;
  ls-tree)
    printf 'OpenPuzzle/CMakeLists.txt\nOpenPuzzle/scripts/build_release.sh\n'
    ;;
  archive)
    output=""
    while [[ "$#" -gt 0 ]]; do
      if [[ "$1" = "-o" ]]; then
        output="$2"
        shift 2
      else
        shift
      fi
    done
    [[ -n "$output" ]]
    printf 'exact committed source archive\n' > "$output"
    ;;
  *)
    echo "Unexpected fake git command: $*" >&2
    exit 1
    ;;
esac
SCRIPT

chmod +x \
  "$FAKE_BIN/cmake" \
  "$FAKE_BIN/ctest" \
  "$FAKE_BIN/cpack" \
  "$FAKE_BIN/git"

OPENPUZZLE_TEST_COMMAND_LOG="$LOG" \
OPENPUZZLE_TEST_REPOSITORY="$TEST_ROOT/repository" \
PATH="$FAKE_BIN:$PATH" \
  "$BUILD_SCRIPT" "$OUTPUT" >/dev/null

DEB="$OUTPUT/OpenPuzzle-$VERSION-Linux-x86_64.deb"
DEB_SHA="$(sha256sum "$DEB" | awk '{print $1}')"
PORTABLE="$OUTPUT/OpenPuzzle-$VERSION-portable-${DEB_SHA:0:8}.deb"

for file in \
  "$DEB" \
  "$OUTPUT/OpenPuzzle-$VERSION-Linux-x86_64.tar.gz" \
  "$PORTABLE" \
  "$OUTPUT/OpenPuzzle-$VERSION-source.tar.gz" \
  "$OUTPUT/RELEASE_MANIFEST.txt" \
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
grep -q '^git .* archive ' "$LOG" || fail "source archive was not created"
grep -Fxq "Commit: 0000000000000000000000000000000000000000" \
  "$OUTPUT/RELEASE_MANIFEST.txt" || fail "release commit was not recorded"
grep -Fxq "Automated tests: 116" \
  "$OUTPUT/RELEASE_MANIFEST.txt" || fail "test count was not recorded"
[[ "$(wc -l < "$OUTPUT/SHA256SUMS.txt")" -eq 5 ]] || \
  fail "updater manifest does not contain five release assets"

if OPENPUZZLE_TEST_COMMAND_LOG="$LOG" \
  OPENPUZZLE_TEST_REPOSITORY="$TEST_ROOT/repository" \
  PATH="$FAKE_BIN:$PATH" \
  "$BUILD_SCRIPT" "$OUTPUT" >/dev/null 2>&1; then
  fail "non-empty output directory was accepted"
fi

DIRTY_OUTPUT="$TEST_ROOT/dirty-output"
if OPENPUZZLE_TEST_COMMAND_LOG="$LOG" \
  OPENPUZZLE_TEST_REPOSITORY="$TEST_ROOT/repository" \
  OPENPUZZLE_TEST_DIRTY=1 \
  PATH="$FAKE_BIN:$PATH" \
  "$BUILD_SCRIPT" "$DIRTY_OUTPUT" >/dev/null 2>&1; then
  fail "dirty Git worktree was accepted"
fi

printf 'corrupt manifest\n' > "$OUTPUT/SHA256SUMS"
if "$VALIDATOR" "$OUTPUT" "$VERSION" >/dev/null 2>&1; then
  fail "corrupt conventional manifest was accepted"
fi

echo "BuildReleaseScriptTests passed"

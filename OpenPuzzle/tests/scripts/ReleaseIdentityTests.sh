#!/usr/bin/env bash
set -euo pipefail

ROOT="${1:?OpenPuzzle source directory is required}"
VERSION="${2:?OpenPuzzle version is required}"

fail() {
  echo "ReleaseIdentityTests: $*" >&2
  exit 1
}

[[ "$VERSION" =~ ^[0-9]+[.][0-9]+[.][0-9]+$ ]] || \
  fail "invalid semantic version: $VERSION"

CMAKE="$ROOT/CMakeLists.txt"
CHANGELOG="$ROOT/CHANGELOG.md"
RELEASE_NOTE="$ROOT/docs/RELEASE_$VERSION.md"

[[ -f "$CMAKE" && ! -L "$CMAKE" ]] || fail "CMakeLists.txt is missing or unsafe"
[[ -f "$CHANGELOG" && ! -L "$CHANGELOG" ]] || fail "CHANGELOG.md is missing or unsafe"
[[ -f "$RELEASE_NOTE" && ! -L "$RELEASE_NOTE" ]] || \
  fail "release note is missing or unsafe: $(basename "$RELEASE_NOTE")"

PROJECT_VERSION="$({
  sed -nE \
    's/^project\(OpenPuzzle VERSION ([0-9]+[.][0-9]+[.][0-9]+) LANGUAGES CXX\)$/\1/p' \
    "$CMAKE"
} || true)"
[[ "$PROJECT_VERSION" = "$VERSION" ]] || \
  fail "project version does not match the CMake test version"

[[ "$(grep -Fc "docs/RELEASE_$VERSION.md" "$CMAKE")" -eq 1 ]] || \
  fail "release note is not installed exactly once"
grep -Fqx "## $VERSION — Reproducible Linux release contract" "$CHANGELOG" || \
  fail "changelog entry is missing"
grep -Fqx "# OpenPuzzle $VERSION — Reproducible Linux release contract" \
  "$RELEASE_NOTE" || fail "release note title is invalid"
grep -Fq "OpenPuzzle-$VERSION-Linux-x86_64.deb" "$RELEASE_NOTE" || \
  fail "release package identity is missing"
grep -Fq "OpenPuzzle-$VERSION-portable-XXXXXXXX.deb" "$RELEASE_NOTE" || \
  fail "portable updater identity is missing"

echo "ReleaseIdentityTests passed"

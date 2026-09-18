#!/usr/bin/env bash
set -euo pipefail

SCRIPT="${1:?create_release_manifests.sh path is required}"
VERSION="9.8.7"
TEST_ROOT="$(mktemp -d -t openpuzzle-release-contract.XXXXXXXX)"
trap 'rm -rf -- "$TEST_ROOT"' EXIT

fail() {
  echo "CreateReleaseManifestsTests: $*" >&2
  exit 1
}

make_packages() {
  local directory="$1"
  mkdir -p "$directory"
  printf 'OpenPuzzle test DEB\n' > \
    "$directory/OpenPuzzle-$VERSION-Linux-x86_64.deb"
  printf 'OpenPuzzle test TGZ\n' > \
    "$directory/OpenPuzzle-$VERSION-Linux-x86_64.tar.gz"
}

DIST="$TEST_ROOT/success"
make_packages "$DIST"

"$SCRIPT" "$DIST" "$VERSION" >/dev/null

DEB="$DIST/OpenPuzzle-$VERSION-Linux-x86_64.deb"
DEB_SHA="$(sha256sum "$DEB" | awk '{print $1}')"
PORTABLE="$DIST/OpenPuzzle-$VERSION-portable-${DEB_SHA:0:8}.deb"
VERSIONED="$DIST/OpenPuzzle-$VERSION-SHA256SUMS.txt"
STABLE="$DIST/SHA256SUMS.txt"
CONVENTIONAL="$DIST/SHA256SUMS"

[[ -f "$PORTABLE" ]] || fail "portable package was not created"
cmp -s "$DEB" "$PORTABLE" || fail "portable package is not byte-identical"
cmp -s "$VERSIONED" "$STABLE" || fail "release manifests differ"
cmp -s "$VERSIONED" "$CONVENTIONAL" || \
  fail "conventional checksum manifest differs"
(
  cd "$DIST"
  sha256sum -c "$(basename "$STABLE")" >/dev/null
) || \
  fail "stable manifest verification failed"

PORTABLE_LINES="$(
  awk '$2 ~ /^OpenPuzzle-[0-9]+\.[0-9]+\.[0-9]+-portable-[0-9a-f]{8}\.deb$/ {n++} END{print n+0}' \
    "$STABLE"
)"
[[ "$PORTABLE_LINES" -eq 1 ]] || \
  fail "stable manifest does not contain exactly one portable package"

printf 'source archive\n' > "$DIST/OpenPuzzle-$VERSION-source.tar.gz"
printf 'release manifest\n' > "$DIST/RELEASE_MANIFEST.txt"
"$SCRIPT" "$DIST" "$VERSION" >/dev/null

[[ "$(wc -l < "$STABLE")" -eq 5 ]] || \
  fail "final manifest does not contain five release artifacts"
(
  cd "$DIST"
  sha256sum -c "$(basename "$STABLE")" >/dev/null
) || fail "final release manifest verification failed"

PORTABLE_INODE="$(stat -c %i "$PORTABLE")"
FIRST_MANIFEST_SHA="$(sha256sum "$STABLE" | awk '{print $1}')"
"$SCRIPT" "$DIST" "$VERSION" >/dev/null
[[ "$(stat -c %i "$PORTABLE")" = "$PORTABLE_INODE" ]] || \
  fail "idempotent rerun replaced the portable package"
[[ "$(sha256sum "$STABLE" | awk '{print $1}')" = "$FIRST_MANIFEST_SHA" ]] || \
  fail "idempotent rerun changed the stable manifest"
cmp -s "$STABLE" "$CONVENTIONAL" || \
  fail "idempotent rerun changed the conventional manifest"

STALE_DIST="$TEST_ROOT/stale"
make_packages "$STALE_DIST"
printf 'stale package\n' > \
  "$STALE_DIST/OpenPuzzle-$VERSION-portable-00000000.deb"
if "$SCRIPT" "$STALE_DIST" "$VERSION" >/dev/null 2>&1; then
  fail "stale portable package was accepted"
fi

MISMATCH_DIST="$TEST_ROOT/mismatch"
make_packages "$MISMATCH_DIST"
MISMATCH_DEB="$MISMATCH_DIST/OpenPuzzle-$VERSION-Linux-x86_64.deb"
MISMATCH_SHA="$(sha256sum "$MISMATCH_DEB" | awk '{print $1}')"
printf 'different package\n' > \
  "$MISMATCH_DIST/OpenPuzzle-$VERSION-portable-${MISMATCH_SHA:0:8}.deb"
if "$SCRIPT" "$MISMATCH_DIST" "$VERSION" >/dev/null 2>&1; then
  fail "non-identical portable package was accepted"
fi

MISSING_DIST="$TEST_ROOT/missing"
mkdir -p "$MISSING_DIST"
printf 'only DEB\n' > \
  "$MISSING_DIST/OpenPuzzle-$VERSION-Linux-x86_64.deb"
if "$SCRIPT" "$MISSING_DIST" "$VERSION" >/dev/null 2>&1; then
  fail "missing TGZ package was accepted"
fi

if "$SCRIPT" "$DIST" "1.0" >/dev/null 2>&1; then
  fail "invalid version was accepted"
fi

INCOMPLETE_DIST="$TEST_ROOT/incomplete"
make_packages "$INCOMPLETE_DIST"
printf 'source only\n' > \
  "$INCOMPLETE_DIST/OpenPuzzle-$VERSION-source.tar.gz"
if "$SCRIPT" "$INCOMPLETE_DIST" "$VERSION" >/dev/null 2>&1; then
  fail "source archive without release manifest was accepted"
fi

echo "CreateReleaseManifestsTests passed"

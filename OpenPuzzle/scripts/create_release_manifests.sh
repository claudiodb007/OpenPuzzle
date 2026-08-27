#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIST="${1:-$ROOT/dist}"
VERSION="${2:-$(sed -nE 's/^project\(OpenPuzzle VERSION ([0-9]+\.[0-9]+\.[0-9]+) LANGUAGES CXX\)$/\1/p' "$ROOT/CMakeLists.txt")}"
[[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "Invalid version" >&2; exit 1; }
DEB="$DIST/OpenPuzzle-$VERSION-Linux-x86_64.deb"
TGZ="$DIST/OpenPuzzle-$VERSION-Linux-x86_64.tar.gz"
[[ -f "$DEB" && -f "$TGZ" ]] || { echo "Missing DEB/TGZ release package" >&2; exit 1; }
mapfile -t P < <(find "$DIST" -maxdepth 1 -type f -name "OpenPuzzle-$VERSION-portable-????????.deb" -printf '%p\n' | sort)
[[ "${#P[@]}" -eq 1 ]] || { echo "Expected exactly one portable package; found ${#P[@]}" >&2; exit 1; }
PORTABLE="${P[0]}"
SHA="$(sha256sum "$PORTABLE" | awk '{print $1}')"
NAME="$(basename "$PORTABLE")"
[[ "$NAME" = "OpenPuzzle-$VERSION-portable-${SHA:0:8}.deb" ]] || { echo "Portable SHA prefix mismatch" >&2; exit 1; }
V="$DIST/OpenPuzzle-$VERSION-SHA256SUMS.txt"
S="$DIST/SHA256SUMS.txt"
for f in "$DEB" "$PORTABLE" "$TGZ"; do
  sha256sum "$f" | awk '{print $1 "  " $2}' | sed "s#  $DIST/#  #"
done > "$V"
cp -f "$V" "$S"
cmp -s "$V" "$S"
[[ "$(awk '$2 ~ /^OpenPuzzle-[0-9]+\.[0-9]+\.[0-9]+-portable-[0-9a-f]{8}\.deb$/ {n++} END{print n+0}' "$S")" -eq 1 ]]
echo "Versioned manifest: $V"
echo "Stable manifest:    $S"

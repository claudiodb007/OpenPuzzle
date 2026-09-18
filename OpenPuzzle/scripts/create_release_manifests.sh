#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIST="${1:-$ROOT/dist}"
VERSION="${2:-$(sed -nE 's/^project\(OpenPuzzle VERSION ([0-9]+\.[0-9]+\.[0-9]+) LANGUAGES CXX\)$/\1/p' "$ROOT/CMakeLists.txt")}"
[[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "Invalid version" >&2; exit 1; }
DEB="$DIST/OpenPuzzle-$VERSION-Linux-x86_64.deb"
TGZ="$DIST/OpenPuzzle-$VERSION-Linux-x86_64.tar.gz"
[[ -f "$DEB" && -f "$TGZ" ]] || { echo "Missing DEB/TGZ release package" >&2; exit 1; }

DEB_SHA="$(sha256sum "$DEB" | awk '{print $1}')"
PORTABLE_NAME="OpenPuzzle-$VERSION-portable-${DEB_SHA:0:8}.deb"
PORTABLE="$DIST/$PORTABLE_NAME"

mapfile -t PORTABLE_PACKAGES < <(
  find "$DIST" -maxdepth 1 -type f \
    -name "OpenPuzzle-$VERSION-portable-????????.deb" \
    -printf '%p\n' | sort
)

if [[ "${#PORTABLE_PACKAGES[@]}" -gt 1 ]]; then
  echo "Expected at most one portable package; found ${#PORTABLE_PACKAGES[@]}" >&2
  exit 1
fi

if [[ "${#PORTABLE_PACKAGES[@]}" -eq 1 ]]; then
  EXISTING="${PORTABLE_PACKAGES[0]}"
  [[ "$EXISTING" = "$PORTABLE" ]] || {
    echo "Stale portable package: $(basename "$EXISTING")" >&2
    exit 1
  }
  cmp -s "$DEB" "$EXISTING" || {
    echo "Portable package differs from the release DEB" >&2
    exit 1
  }
else
  TEMP_PORTABLE="$(mktemp "$DIST/.openpuzzle-portable.XXXXXXXX")"
  trap 'rm -f -- "$TEMP_PORTABLE"' EXIT
  cp -- "$DEB" "$TEMP_PORTABLE"
  chmod 0644 "$TEMP_PORTABLE"
  mv -- "$TEMP_PORTABLE" "$PORTABLE"
  trap - EXIT
fi

PORTABLE_SHA="$(sha256sum "$PORTABLE" | awk '{print $1}')"
[[ "$PORTABLE_SHA" = "$DEB_SHA" ]] || {
  echo "Portable package checksum differs from the release DEB" >&2
  exit 1
}
[[ "$PORTABLE_NAME" = "OpenPuzzle-$VERSION-portable-${PORTABLE_SHA:0:8}.deb" ]] || {
  echo "Portable SHA prefix mismatch" >&2
  exit 1
}

V="$DIST/OpenPuzzle-$VERSION-SHA256SUMS.txt"
S="$DIST/SHA256SUMS.txt"
C="$DIST/SHA256SUMS"
SOURCE="$DIST/OpenPuzzle-$VERSION-source.tar.gz"
RELEASE_MANIFEST="$DIST/RELEASE_MANIFEST.txt"

if [[ -e "$SOURCE" || -e "$RELEASE_MANIFEST" ]]; then
  [[ -f "$SOURCE" && ! -L "$SOURCE" && \
     -f "$RELEASE_MANIFEST" && ! -L "$RELEASE_MANIFEST" ]] || {
    echo "Source archive and release manifest must be created together" >&2
    exit 1
  }
  RELEASE_FILES=("$DEB" "$TGZ" "$SOURCE" "$RELEASE_MANIFEST" "$PORTABLE")
else
  RELEASE_FILES=("$DEB" "$TGZ" "$PORTABLE")
fi

for f in "${RELEASE_FILES[@]}"; do
  printf '%s  %s\n' \
    "$(sha256sum "$f" | awk '{print $1}')" \
    "$(basename "$f")"
done > "$V"
cp -f "$V" "$S"
cp -f "$V" "$C"
cmp -s "$V" "$S"
cmp -s "$V" "$C"
[[ "$(awk '$2 ~ /^OpenPuzzle-[0-9]+\.[0-9]+\.[0-9]+-portable-[0-9a-f]{8}\.deb$/ {n++} END{print n+0}' "$S")" -eq 1 ]] || {
  echo "Stable manifest must contain exactly one portable package" >&2
  exit 1
}
echo "Portable package:  $PORTABLE"
if [[ -f "$SOURCE" ]]; then
  echo "Source archive:    $SOURCE"
  echo "Release manifest:  $RELEASE_MANIFEST"
fi
echo "Versioned manifest: $V"
echo "Updater manifest:   $S"
echo "Checksum manifest:  $C"

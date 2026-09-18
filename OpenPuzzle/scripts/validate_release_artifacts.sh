#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIST="${1:-$ROOT/dist}"
VERSION="${2:-$(sed -nE 's/^project\(OpenPuzzle VERSION ([0-9]+\.[0-9]+\.[0-9]+) LANGUAGES CXX\)$/\1/p' "$ROOT/CMakeLists.txt")}"

[[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || {
  echo "Invalid version" >&2
  exit 1
}
[[ -d "$DIST" ]] || {
  echo "Release directory not found: $DIST" >&2
  exit 1
}

DEB="$DIST/OpenPuzzle-$VERSION-Linux-x86_64.deb"
TGZ="$DIST/OpenPuzzle-$VERSION-Linux-x86_64.tar.gz"
VERSIONED="$DIST/OpenPuzzle-$VERSION-SHA256SUMS.txt"
UPDATER="$DIST/SHA256SUMS.txt"
CONVENTIONAL="$DIST/SHA256SUMS"

for file in "$DEB" "$TGZ" "$VERSIONED" "$UPDATER" "$CONVENTIONAL"; do
  [[ -f "$file" && ! -L "$file" ]] || {
    echo "Missing or unsafe release artifact: $(basename "$file")" >&2
    exit 1
  }
done

mapfile -t PORTABLE_PACKAGES < <(
  find "$DIST" -maxdepth 1 -type f \
    -name "OpenPuzzle-$VERSION-portable-????????.deb" \
    -printf '%p\n' | sort
)
[[ "${#PORTABLE_PACKAGES[@]}" -eq 1 ]] || {
  echo "Expected exactly one portable package; found ${#PORTABLE_PACKAGES[@]}" >&2
  exit 1
}

DEB_SHA="$(sha256sum "$DEB" | awk '{print $1}')"
PORTABLE="$DIST/OpenPuzzle-$VERSION-portable-${DEB_SHA:0:8}.deb"
[[ "${PORTABLE_PACKAGES[0]}" = "$PORTABLE" ]] || {
  echo "Portable package filename does not match the DEB SHA-256 prefix" >&2
  exit 1
}
cmp -s "$DEB" "$PORTABLE" || {
  echo "Portable package differs from the release DEB" >&2
  exit 1
}

EXPECTED="$(mktemp "$DIST/.openpuzzle-release-manifest.XXXXXXXX")"
trap 'rm -f -- "$EXPECTED"' EXIT
for file in "$DEB" "$PORTABLE" "$TGZ"; do
  printf '%s  %s\n' \
    "$(sha256sum "$file" | awk '{print $1}')" \
    "$(basename "$file")"
done > "$EXPECTED"

for manifest in "$VERSIONED" "$UPDATER" "$CONVENTIONAL"; do
  cmp -s "$EXPECTED" "$manifest" || {
    echo "Invalid release manifest: $(basename "$manifest")" >&2
    exit 1
  }
done

(
  cd "$DIST"
  sha256sum -c "$(basename "$UPDATER")" >/dev/null
)

echo "Release artifacts validated."
echo "Version............ $VERSION"
echo "Portable package... $(basename "$PORTABLE")"
echo "Required files..... 6/6"

#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIST="${1:-$ROOT/dist}"
VERSION="${2:-$(sed -nE 's/^project\(OpenPuzzle VERSION ([0-9]+\.[0-9]+\.[0-9]+) LANGUAGES CXX\)$/\1/p' "$ROOT/CMakeLists.txt")}"
REPOSITORY="${3:-}"
COMMIT="${4:-}"

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
SOURCE="$DIST/OpenPuzzle-$VERSION-source.tar.gz"
RELEASE_MANIFEST="$DIST/RELEASE_MANIFEST.txt"

for file in \
  "$DEB" "$TGZ" "$SOURCE" "$RELEASE_MANIFEST" \
  "$VERSIONED" "$UPDATER" "$CONVENTIONAL"; do
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
EXPECTED_SOURCE=""
cleanup() {
  rm -f -- "$EXPECTED"
  [[ -z "$EXPECTED_SOURCE" ]] || rm -f -- "$EXPECTED_SOURCE"
}
trap cleanup EXIT
for file in "$DEB" "$TGZ" "$SOURCE" "$RELEASE_MANIFEST" "$PORTABLE"; do
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

grep -Eq "^OpenPuzzle release: $VERSION$" "$RELEASE_MANIFEST"
grep -Eq '^Branch: .+$' "$RELEASE_MANIFEST"
grep -Eq '^Commit: [0-9a-f]{40}$' "$RELEASE_MANIFEST"
grep -Eq '^Commit date: .+$' "$RELEASE_MANIFEST"
grep -Eq '^Source tracked files: [1-9][0-9]*$' "$RELEASE_MANIFEST"
grep -Eq '^Automated tests: [1-9][0-9]*$' "$RELEASE_MANIFEST"
grep -Fxq "Debian package: $(basename "$DEB")" "$RELEASE_MANIFEST"
grep -Fxq "Binary archive: $(basename "$TGZ")" "$RELEASE_MANIFEST"
grep -Fxq "Portable updater package: $(basename "$PORTABLE")" "$RELEASE_MANIFEST"
grep -Fxq "Source archive: $(basename "$SOURCE")" "$RELEASE_MANIFEST"
grep -Eq '^CUDA engine SHA-256: [0-9a-f]{64}$' "$RELEASE_MANIFEST"
grep -Eq '^OpenCL engine SHA-256: [0-9a-f]{64}$' "$RELEASE_MANIFEST"
grep -Eq '^KeyHunt engine SHA-256: [0-9a-f]{64}$' "$RELEASE_MANIFEST"
grep -Fxq 'Tag: not created' "$RELEASE_MANIFEST"
grep -Fxq 'Remote push: not performed' "$RELEASE_MANIFEST"

if [[ -n "$REPOSITORY" || -n "$COMMIT" ]]; then
  [[ "$COMMIT" =~ ^[0-9a-f]{40}$ ]] &&
    git -C "$REPOSITORY" rev-parse --is-inside-work-tree >/dev/null 2>&1 || {
    echo "Repository and commit must be supplied together" >&2
    exit 1
  }
  grep -Fxq "Commit: $COMMIT" "$RELEASE_MANIFEST"
  EXPECTED_SOURCE="$(mktemp "$DIST/.openpuzzle-source-archive.XXXXXXXX")"
  git -C "$REPOSITORY" archive \
    --format=tar.gz \
    --prefix="OpenPuzzle-$VERSION-source/" \
    -o "$EXPECTED_SOURCE" \
    "$COMMIT"
  cmp -s "$SOURCE" "$EXPECTED_SOURCE" || {
    echo "Source archive does not match the release commit" >&2
    exit 1
  }
fi

(
  cd "$DIST"
  sha256sum -c "$(basename "$UPDATER")" >/dev/null
)

echo "Release artifacts validated."
echo "Version............ $VERSION"
echo "Source archive...... $(basename "$SOURCE")"
echo "Portable package... $(basename "$PORTABLE")"
echo "Required files..... 8/8"

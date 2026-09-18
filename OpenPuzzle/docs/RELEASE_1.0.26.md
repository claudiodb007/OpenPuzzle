# OpenPuzzle 1.0.26 — Reproducible Linux release contract

OpenPuzzle 1.0.26 makes the complete Linux release process reproducible and
self-validating. The client, engines and coordination protocol retain the
behavior shipped in OpenPuzzle 1.0.25.

## One-command release pipeline

The release pipeline now performs the complete production sequence with one
command:

    ./scripts/build_release.sh

It configures a clean Release build, builds the command-line client and the
optional Qt desktop interface, runs the complete automated suite, creates the
Debian and binary archives, produces the portable updater package and validates
the final output.

Release builds are refused when the Git worktree contains committed,
uncommitted or untracked changes. This prevents a package from containing code
that cannot be identified by its recorded commit.

## Updater artefact contract

The normal Debian package is copied byte-for-byte to the SHA-prefixed portable
name expected by `openpuzzle update`:

    OpenPuzzle-1.0.26-portable-XXXXXXXX.deb

The eight required release files are produced and validated together:

- Debian installation package;
- binary TGZ archive;
- SHA-prefixed portable updater package;
- exact committed-source archive;
- release provenance manifest;
- versioned SHA-256 manifest;
- stable `SHA256SUMS.txt` updater manifest;
- conventional `SHA256SUMS` manifest.

The three checksum manifests are byte-identical and cover the five published
release artefacts: Debian package, binary archive, source archive, provenance
manifest and portable updater package.

## Source provenance

The source archive is created with `git archive` directly from the release
commit. Generated build and CTest runtime files are excluded from the tracked
source tree.

`RELEASE_MANIFEST.txt` records:

- version, branch, commit and commit date;
- number of tracked source files and automated tests;
- exact names of the binary, portable and source packages;
- SHA-256 identities of the CUDA, OpenCL and KeyHunt engines;
- explicit confirmation that tagging and remote publication have not yet
  occurred during the local build.

The validator independently regenerates the committed-source archive and
requires byte-for-byte identity with the published source package.

## Compatibility

- Existing OpenPuzzle configuration and client identity are retained.
- BitCrack CUDA and OpenCL, KeyHunt CPU and optional PSCKangaroo routing are
  unchanged.
- The desktop interface and command-line controls are unchanged.
- The coordination API and assignment protocol are unchanged.
- Existing OpenPuzzle 1.0.25 clients remain compatible with the server.

## Validation

The isolated suite contains 117 tests, including direct contracts for portable
package generation, the complete release pipeline and the 1.0.26 release
identity.

The production pipeline additionally verifies the exact source commit, package
checksums, release metadata and engine hashes without modifying installed
OpenPuzzle binaries or active runtime state.

## Upgrade

Allow active assignments to finish with `openpuzzle safestop` before replacing
the installed package:

    sudo apt install ./OpenPuzzle-1.0.26-Linux-x86_64.deb

After installation, confirm the release and updater contract:

    openpuzzle --version
    openpuzzle doctor
    openpuzzle update --check

# OpenPuzzle 1.0.49 — Vanity in the common Linux runtime

OpenPuzzle 1.0.49 adds the validated Vanity CUDA engine to the same installed
runtime, desktop controls, assignment lifecycle and status telemetry used by
BitCrack, KeyHunt and Kangaroo. It does not add a second launcher or a separate
production client.

## Vanity CUDA

- `Vanity / CUDA` is available directly in the normal execution selector.
- Start, Stop, Resume and Safe Stop use the ordinary RunSession and persisted
  assignment state.
- Claim, progress and completion use the existing range API. Progress renews
  the lease and completion remains idempotent.
- A process loss resumes the same assignment from verified native coverage;
  it does not repeat a claim or a completed interval.
- The live speed shown by the shared status card is calculated from the
  adapter's monotonic elapsed time. It is not the planning value and does not
  depend on how quickly the UI rereads the log.
- The first completed assignment creates the normal per-GPU `Vanity` profile.
  Later assignments reuse and adapt that profile through the existing GPU
  profile database. No private measurement path is distributed.
- The initial CUDA geometry is the validated `grid=1024`, while the native
  assignment budget remains fixed at `2^36` keys. Provisional `grid=32`
  profiles created by pre-release candidates are migrated after completion.

The packaged Vanity engine is CUDA-only in this release. It supports linear
P2PKH puzzles 65 through 80. The selected physical CUDA device is mapped to
engine device zero in the same owned execution, while the persisted state keeps
the physical device number for recovery and telemetry.

## Packaging and safety

The Linux DEB and TGZ include the vetted Vanity engine, its native adapter and
third-party license under the existing OpenPuzzle `libexec` layout. User
profiles, checkpoints, reports, private keys and local evidence are never
included in release archives.

Production servers must use HTTPS. Numeric loopback HTTP remains available for
isolated tests. The normal thermal observer and explicit GPU controls remain in
force; this release does not change power limits, clocks or fan settings.

## Candidate artifacts

- `OpenPuzzle-1.0.49-Linux-x86_64.deb`
- `OpenPuzzle-1.0.49-Linux-x86_64.tar.gz`
- `OpenPuzzle-1.0.49-portable-XXXXXXXX.deb` (identical DEB; checksum prefix)
- `OpenPuzzle-1.0.49-source.tar.gz`
- Release manifest and SHA-256 files.

Before publication, validate the affected native and desktop suites, run one
ordinary CUDA acceptance assignment with Stop/Resume and Safe Stop, inspect the
package contents, and verify the production API preflight. Do not repeat the
historical benchmark solely for this release.

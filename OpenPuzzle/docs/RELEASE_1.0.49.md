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
- Before the first assignment on a GPU, the common automatic benchmark measures
  a bounded matrix of power-of-two Vanity grids, validates the best sustained
  result and stores it as the normal `GPU + CUDA + Vanity` profile. No claim,
  heartbeat or assignment request is made until that profile exists.
- Later assignments reuse and adapt the measured speed through the existing GPU
  profile database. The desktop Benchmark button uses the same CLI path and can
  deliberately remeasure the selected GPU; cancellation or failure preserves
  the previous profile.
- Geometry is therefore selected per GPU rather than fixed at `grid=1024`.
  Threads and points remain at the engine contract values (`256` and `1024`),
  while assignment size follows measured speed and the selected duration using
  the existing API limit. The adapter runs consecutive native pieces within
  that one assignment; Stop/Resume preserves the exact checked prefix, and
  Safe Stop finishes the whole assignment before blocking the next claim.
  Provisional `grid=32` profiles from pre-release candidates are ignored
  and trigger the automatic benchmark.

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

Before publication, validate the affected native and desktop suites, allow the
candidate to perform its one required automatic Vanity benchmark on the target
GPU, then run one ordinary CUDA acceptance assignment with Stop/Resume and Safe
Stop. Inspect the package contents and verify the production API preflight.

# Experimental Vanity CLI and profiles

Enable with `-DOPENPUZZLE_ENABLE_VANITY_PREVIEW=ON` on Linux. It is OFF by
default. This adds the `openpuzzle vanity-preview` command and a companion
`openpuzzle-vanity-adapter` build target. It does not change the application
version or install/package this experimental engine.

The existing BitCrack, KeyHunt and Kangaroo command paths remain available.
Vanity is intended for compressed P2PKH linear address/hash160 search, like
Puzzle 71. Kangaroo remains the public-key interval solver for puzzles such
as 140. The current engine's known-key restriction is a test harness
restriction, not a requirement of VanitySearch's search algorithm.

## Commands

`openpuzzle vanity-preview help` lists the exact syntax.

- `profile-import` requires explicit experimental opt-in, a complete measured
  manifest and a new absolute destination in an owner-only directory.
- `profile-show` reads the independent profile without GPU execution or DB
  initialization.
- `verify` checks executable hashes, the known synthetic engine identity and
  the preserved evidence hashes; it does not launch a GPU kernel.
- `local-test` requires explicit opt-in and a matching CUDA-visible physical
  GPU UUID. It invokes the same registry/factory/continuous runtime and
  solution export route used in integration preview 0.4.0, now inside the
  OpenPuzzle CLI executable. Its coordinator remains an in-process fixture.

Profiles retain the median/min/max of the three measured runs and an
explicit 3% planning margin. They identify the engine, adapter, GPU UUID,
geometry and evidence files. These are local consistency fingerprints,
not a digital signature or proof that an arbitrary manifest is trustworthy.
The preparation script separately verifies the prior range, performance
and integration reports before creating the import manifest.

The measurement-adapter hash is retained separately from the current adapter
hash. The speed is a reference from the prior measured comparison, not a new
measurement of this complete CLI; production use will require calibration
through the final execution path.

Profiles are stored separately from the SQLite GPU profile table. They do
not overwrite BitCrack tuning and are not selected for production by an
automatic dispatcher. Changed or damaged destination files are preserved.
An incomplete write remains a damaged file for review, never an accepted
profile. Reimporting an identical private profile is idempotent.

## Validation and boundaries

CTest adds four suites: model, protocol, adapter process (12 cases), and CLI
(7 cases including 17 scenario stages). CTest uses the actual OpenPuzzle
binary for the CLI suite. A dependency-light command driver also permits
development testing without the complete application dependencies; passing
that driver does not establish that the complete application built.

The adapter ignores SIGPIPE and treats failure to write progress as a
cancellation request so a closed controller pipe cannot interrupt durable
checkpoint publication. Parent-death cancellation remains active.

`run --engine vanity` and `run --engine vanity-preview` are rejected before
claiming. Production HTTP dispatch, arbitrary address targets, UI selection,
Windows support and distribution remain pending. In particular, do not
rename the synthetic engine or remove its gate to enable production work.

The earlier +74.3% result is from the independent RTX 4070 SUPER comparison
with checkpoints. No performance claim is inferred from these CLI tests.
The observed BitCrack unaligned-tail issue remains unresolved.

OpenPuzzle code is MIT licensed. Python test helpers derived from the
earlier experimental kits are GPLv3; their license is in
`experimental/vanity-preview/tests/support/LICENSE`. The CUDA VanitySearch
engine and its GPL source are reused from the validated local report and
are not included in this source patch.

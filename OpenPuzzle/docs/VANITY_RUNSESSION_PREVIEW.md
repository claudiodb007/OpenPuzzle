# Vanity RunSession and main controls — local preview 0.12.0

Base: validated coordinator commit `20e3964bae9a33faed76652a2a9ce70db3ea142b`.
This connects the existing main-window controls to the native coordinator through
`RunSession::run`. Production Vanity assignments remain disabled.

The existing local profile/benchmark and earlier measured-test routes remain
available. Only the prepared launcher selects the coordinator desktop mode.
There is no new server panel, automatic tuning, new measurement or release.

## Lifecycle

The main Start and Resume actions enter `run 20 --engine vanity --backend cuda
--vanity-session start|resume` with an explicit store, owned run directory and
local opt-in. The read-only run listing uses the same RunSession entry. A strict
argument gate rejects unknown/repeated options, mixed routes, production puzzle
selection and non-CUDA selection before ordinary client recovery or networking.

The owned desktop helper uses the measured physical UUID in both arguments and
CUDA_VISIBLE_DEVICES. The native child also enters RunSession, using a separate
strict local route, and reuses the previously validated coordinator and common
HTTP client. The endpoint remains 127.0.0.1 and the assignment stays synthetic.

One assignment spans two exact consecutive segments. Stop addresses only this
operation and preserves contiguous native checkpoints. Safe Stop finishes both
segments of the current assignment and permits no next claim. Confirmed resume
uses the same client, assignment, range, executables and loopback port; its server
refuses all claims. Reopening the main window lists sealed interrupted runs.

A seal binds the profile, frontend, native arguments, source identities, public
target, native per-batch coverage and cumulative HTTP history. Resume rejects
stale confirmations, damaged checkpoints/evidence, occupied ports, busy native
ownership, changed hardware/profile/source/executables and completed runs. Before
resume, original evidence is archived. A failed process spawn preserves the
original cancel marker and seal; an ambiguous active transaction requires review.
Failed/solution-bearing native outcomes are retained for review rather than
silently treated as resumable. Automatic recovery of a killed desktop helper or
all HTTP failure classes is not enabled by this preview.

Stopping after allocation but before the first kernel can resume that assignment.
Stopping at the final batch can resume only completion, without another kernel.
The helper's GPU checks retain the existing idle-GPU, power-context and 85 C guard.

## UI and validation

The same selector, layout, languages and main controls are used. Button state is
applied once per update, preserving the held-click Stop correction. Benchmark
keeps its separate measured-profile route and does not launch the coordinator.
Ordinary builds retain their original five execution modes.

Here the native RunSession entry, coordinator, real HTTP client and adapter were
compiled and exercised with a synthetic engine. The ordinary production runner
was a development harness boundary, not an alternative production implementation.
The actual MainWindow and panel were compiled with Qt and exercised with a
synthetic CLI in all four languages, including held Stop, Safe Stop, cancellation,
close guarding, reopening and confirmed resume. No real GPU was used here.

The complete build (192 Linux suites and 24 isolated platform suites) and the real
GPU/main-window review must run on the user's Ubuntu using the prepared kit.
Production pacing, automatic production assignment acquisition, broader hardware,
Windows, packaging and publication remain separate work.

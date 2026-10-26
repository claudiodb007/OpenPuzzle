# Vanity in the main Linux window — preview 0.10.0

Vanity / CUDA is now available in the main engine selector when
`OPENPUZZLE_ENABLE_VANITY_PREVIEW` is enabled. The separate Tools launcher is
removed. The measured profile, saved interrupted runs, progress and output are
embedded below the selection. The existing Start, Benchmark, Safe Stop and Stop
buttons route to the measured local workflow; Resume is beside Start.

This is preparation for the planned 1.0.49 integration, not a release or a
production enablement. Start is explicitly labelled as a local test. Puzzle,
device and automatic-start fields are hidden for this fixed-profile local route.
The synthetic assignment, physical GPU binding and measured executable identity
remain unchanged. Benchmark repeats the existing measured comparison; it does
not run BitCrack's automatic grid tuner for Vanity. Confirmations explain this.

## Controls and isolation

- During an owned command, engine switching and other commands are disabled.
  Stop and Safe Stop address the owned local operation, never the production CLI.
- Close is blocked until the owned process finishes. Cancelling a benchmark
  preserves the previous profile through the existing measured-profile backend.
- The main runtime status still gates new operations. A running, unknown or
  solution-bearing production state prevents a new local test/measurement.
- Profile errors block Start, Resume and Benchmark. A missing store is not
  silently replaced by the installed application's profile database.
- Language can change while running. It changes captions and cached display
  data without launching another command or changing paths/protocol fields.
- Production thermal settings are disabled while Vanity is selected. The local
  workflow retains its existing 85 °C stop, power-context checks and idle-GPU
  requirement. GPU writes are disabled while an owned command runs.
- Standard builds without the preview flag retain the five existing modes.

## Evidence and tests

The preceding 0.9.0 report has 186 passed suites reported by the user. A real
resume was reported complete from 17,985,175,552 to 68,719,476,736 checked keys.
The preparation script verifies its seal, exact native coverage, archived
interrupted state, HTTP history, two launches, and one claim before building.
The original report is read only and is not copied into the new run store.

`UiVanityIntegrationTests` exercises the real MainWindow and embedded panel with
a synthetic CLI: selector, all common actions, confirmation refusal, live
progress/languages, close guard, cancellation, reopen/resume, failed profiles,
occupied-runtime blocking, and return to existing modes. The full Linux suite
count becomes 187. The standalone panel tests and existing ordinary UI tests
remain registered. No GPU or production server is used by these tests.

Manual review of this new main-window route remains required. Production
coordinator integration, final release numbering, packaging and publication are
separate work and are not performed by this preview.

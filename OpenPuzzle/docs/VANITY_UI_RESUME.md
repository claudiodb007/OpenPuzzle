# Vanity local desktop resume — preview 0.9.0

This extends the validated experimental feature commit
`c9289b66dc6524ff2953aaf98338846387a746f8`. It does not enable production runs,
install a package, change GPUs or publish a release.

## Desktop flow

Open the prepared isolated preview, then Tools → Vanity / CUDA — local preview.
Run a local test, wait for nonzero progress, and press Stop. Once cancellation
finishes, select that interrupted run and press **Resume local run**. The
confirmation displays the preserved key count. The same run remains selectable
after closing and reopening the panel. Resume may itself be cancelled again.
Safe Stop during resume finishes the current interval and then stops.

Other GPU searches must be stopped before a confirmed local test or resume.
Power and fans remain unchanged. GPU identity, measured driver/power context,
temperature and competing compute activity are checked as in preview 0.8.0.

## Scope and evidence

New local runs are sealed only after the measured CLI exits with a coherent
native checkpoint, HTTP journal, target, exact arguments and engine-attempt
logs. The seal records file hashes and the independent measured-profile binding.
Only sealed interrupted runs under this preview's own store can resume. Old
0.8.0 reports are preserved and are not adopted automatically.

Resume retains the original paths, synthetic public target, range, GPU, measured
CLI, adapter, engine and loopback port. If the port is occupied, resume refuses
before changing run data. The resumed local HTTP fixture rejects any claim and
accepts progress from the recorded offset; it completes only at the exact end.
The CLI's persistent launch count and every engine's exact interval/progress
records are checked together. Completed and solution-bearing runs cannot resume.

Cancellation can close the engine output stream before a STOPPED record arrives.
In that case the adapter's durable interrupted result and contiguous acknowledged
batches determine the resume offset. Completion still requires the engine's
COMPLETE record. Unacknowledged work may be recomputed; acknowledged intervals
must not overlap or leave gaps.

Before clearing only that run's cancel marker, resume archives its original
records in `resume-history`. Each attempt keeps its process log and HTTP history.
A stale UI confirmation, altered source/profile/executables, malformed checkpoint,
unsafe path, busy native owner, or changed evidence causes refusal. The global
preview lock prevents concurrent commands within the same store. Failed process
creation restores the cancel marker; an ambiguous interrupted resume transaction
is retained for manual review rather than automatically relaunched.

## CLI

- `vanity-preview measured-runs --store DIR`: read and list eligible local runs.
- `vanity-preview measured-resume --store DIR --run-root DIR --expected-state SHA256 --enable-experimental-vanity-local`:
  resume only the exact seal reviewed by the caller.

The desktop invokes these with literal arguments. It blocks closing during an
owned operation, retains Stop/Safe Stop, and displays speed for the current
attempt while showing cumulative interval progress.

## Validation boundaries

The added native integration suite uses the actual CLI and adapter with a fake
engine; it covers cancellation/resume, repeated cancellation, Safe Stop, exact
coverage, a single historical claim, damaged data, stale confirmation, occupied
ports, busy ownership, failed-spawn restoration and blocked replay. Qt tests cover
four languages, reopening, selection, declining confirmation and resume routing.
The full Linux suite must pass when preparing the user preview. Real GPU resume
and visual confirmation remain separate manual validation; prior measurements
are reused without assigning their identity to rebuilt executables.

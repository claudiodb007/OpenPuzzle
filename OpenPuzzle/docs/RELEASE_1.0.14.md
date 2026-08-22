# OpenPuzzle 1.0.14

OpenPuzzle 1.0.14 adds conservative adaptive calibration for automatically
managed GPU profiles. Real completed work can now refine future range sizing
without changing the simple `openpuzzle run` workflow.

## Sustained real-world calibration

After a confirmed successful CUDA or OpenCL assignment, OpenPuzzle reads the
speed samples recorded by the engine. It ignores the first two valid warm-up
readings, requires at least five sustained readings and uses their median so a
short spike cannot dominate the result.

The measured median receives a 0.97 planning factor. The new estimate is then
blended at 25% with 75% of the previous profile and limited to a maximum change
of 15% per completed assignment. Minimum, maximum and sample history are kept
with the same exact GPU, backend and engine profile.

## Launch settings stay stable

Adaptive calibration updates only the planning speed. The benchmark-selected
blocks, threads and points are preserved and must match the completed launch.
The feature therefore learns from real sustained performance without silently
retuning a stable engine configuration.

## Strict completion boundary

Calibration occurs only after assignment completion has been accepted. CPU
work, manually configured GPU launches, failures, cancellations and runtime
states created by older clients are skipped safely. A calibration error cannot
turn a successfully completed assignment into a failure.

## Fair scheduling remains unchanged

Every OpenPuzzle client keeps the same right to receive work. Performance data
is used only to size a future range for the target duration; it never changes
queue priority, reputation, assignment access or scheduling preference.

## Compatibility

Existing profiles remain valid. OpenPuzzle 1.0.14 begins learning only from
new automatically managed GPU assignments that carry the required profile and
launch identity. No migration or manual profile reset is required.

## Validation

The release was compiled from source and all 88 automated tests passed,
including focused calibration, profile update, state persistence, completion
and client-runtime regression tests. Validation was performed while the
installed OpenPuzzle 1.0.13 CUDA runtime continued undisturbed.

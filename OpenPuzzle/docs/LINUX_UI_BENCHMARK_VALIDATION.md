# Linux desktop benchmark validation

Source base: `85946b2bcc76dc143dd69bc7b3a40a9ccdcf0752`.
Preview report: `linux-ui-benchmark/preview-q8ju4sx9`.
Patch SHA-256: `75affc56016d2787af918272931bf244a20709fea197816356fe9ae609f24869`.

## Automated evidence

The user's Ubuntu preview report confirms 178/178 native Linux suites and
24/24 isolated portable suites. CTest registration and JUnit are checked again
when committing, without rerunning the tests. Source hashes must match the
validated preview exactly.

Covered fixtures include Qt confirmation and device/backend routing, automatic
synthetic tuning, SQLite profile save, preservation on failure/cancellation,
owned-child shutdown, workspace locking and retry. These are synthetic tests,
not proof of a particular GPU's performance.

## Manual evidence

On 2026-10-20, after preparing this preview, the user confirmed:
“benchmark is working great”. This records a successful real benchmark through
the Linux interface. The message supplies no backend, speed, launch parameters
or separate real-hardware cancellation result; none is inferred here.

This is a feature integration, not a release or installation. The integration
script does not run benchmarks, change GPU settings, start searches, move tags
or publish remotely. Linux 1.0.47 and the existing Windows test release are not
changed by this local commit.

# Linux desktop automatic GPU benchmark

The Qt Benchmark button now opens a confirmation dialog for CUDA/OpenCL, the
engine's device index and (for OpenCL) optional Rusticl radeonsi. It invokes the
existing Linux automatic tuning algorithm with `--auto --real --gpu N`; launch
parameters are not hard-coded by the panel. The shared candidate matrix,
stability selection, finalist with a 60-second limit and 3% planning margin are unchanged.

The dialog explains that this is a local measurement, normally 10–20 minutes,
with no production assignment. Cancel is the default dialog action. The panel
rechecks command availability after confirmation. An active search, unresolved
process identity or a GPU-control write blocks the panel's benchmark route.

## Progress and cancellation

The owning QProcess streams bounded UTF-8 output to Messages. During measurement,
the Stop button becomes Cancel benchmark, launch controls and GPU writes are
disabled, and closing the panel asks the user to cancel or wait first.
Cancellation sends SIGTERM only to the owned benchmark CLI. The CLI passes a
cooperative predicate to its process runner, which stops the owned engine process
group even if the engine is silent. A cancelled measurement returns 130 and is
never accepted as a successful sample set or written as a GPU profile.

The UI includes `--require-idle`. This local-state guard runs before measurement,
between candidates, before the finalist and before saving. It is opt-in so that
the existing first-profile preparation inside a supervised CLI run is unchanged.
It does not reserve a GPU against other applications or processes launched
externally after a check. A private, no-follow advisory workspace lock prevents
two new-version benchmarks from clearing or sharing the same synthetic output.

Only a successful finalist can replace the matching GPU/backend/engine database
profile. Cancellation and failed measurement preserve the previous profile. No
power, fan or clock setting is changed by the benchmark. An explicitly selected
Rusticl setting retains the existing CLI persistence behavior.

## Validation

- `UiBenchmarkTests`: actual offscreen Qt dialog, cancel/confirm, exact CUDA and
  OpenCL argv, selected device, Rusticl isolation, streamed split UTF-8, disabled
  launch/GPU controls, close guard, owned cancellation, failure and successful
  completion using a synthetic CLI.
- `UiControlTests`: regression now accepts the dialog and verifies automatic
  real-measurement arguments instead of the former preview-only command.
- `BenchmarkRunnerExecutionTests`: positive partial samples cannot make a
  cancellation successful; silent child shutdown and cancellation before launch.
- `LinuxBenchmarkCliTests`: actual CLI with synthetic bundled engine and inventory,
  automatic selection/finalist, SQLite profile save, failure/cancel preservation,
  concurrent and unsafe lock rejection, child shutdown and retry. No hardware or
  server is used.

This feature is based on `85946b2bcc76dc143dd69bc7b3a40a9ccdcf0752` (validated
Windows automatic benchmarks). It does not change the release version or publish
packages. Real Ubuntu GPU measurement and visual review must be recorded
separately from automated fixtures before release.

# OpenPuzzle 1.0.13

OpenPuzzle 1.0.13 introduces comprehensive, actionable diagnostics through
`openpuzzle doctor`.

## Complete local readiness

The doctor command now reports:

- installed version and local configuration;
- private local storage availability and write access;
- bundled CUDA, OpenCL and CPU engines;
- detected processors and GPU devices;
- usable execution backends;
- selected benchmark profile and optional profiles;
- active, preserved and stale runtime state.

The selected GPU backend determines which benchmark profile is required. A
missing profile for an unused backend is reported as optional and does not
produce a false warning.

## Safe server reachability

By default, the doctor performs a lightweight HTTPS `HEAD` request to the
configured coordination server. It does not request an assignment, start an
engine, stop active work or transmit private solution data.

Use the fully local mode when network access is unavailable:

```bash
openpuzzle doctor --offline
```

## Clear results and recovery actions

The final summary distinguishes `READY`, `READY WITH WARNINGS` and `NOT READY`.
Stable codes `OP-DOCTOR-001` through `OP-DOCTOR-006` identify missing backends,
configuration issues, storage failures, stale runtime state, missing selected
profiles and server reachability failures. Each finding includes a concrete
next action.

## Validation

The release was compiled from source and all 86 automated tests passed. The
doctor was also validated in online mode, offline mode and with a controlled
network failure while an active runtime continued undisturbed.

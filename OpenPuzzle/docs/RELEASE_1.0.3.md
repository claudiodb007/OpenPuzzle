# OpenPuzzle 1.0.3

OpenPuzzle 1.0.3 adds an optional systemd user service for continuous
Linux clients.

## Highlights

- Installs the `openpuzzle@.service` user-service template.
- Supports one selected `cuda`, `opencl` or `cpu` backend per user.
- Reads optional per-backend arguments from
  `~/.config/OpenPuzzle/<backend>.env`.
- Runs without keeping a terminal open.
- Restarts the client after unexpected failures.
- Preserves graceful `openpuzzle stop` handling.
- Uses a three-minute shutdown limit with a final process-group
  fallback.
- Supports explicit administrator-controlled lingering for headless
  machines.

## Scope

This release does not silently enable or start a service. Installation
does not stop, replace or modify an active OpenPuzzle runtime. The user
must explicitly select and enable one backend.

Concurrent GPU and CPU service instances are outside the 1.0.3 service
scope. Existing foreground concurrent execution remains available.

See [SYSTEMD_SERVICE.md](SYSTEMD_SERVICE.md) for activation, status,

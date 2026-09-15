# OpenPuzzle 1.0.25 — Desktop control and resilient runtime inspection

OpenPuzzle 1.0.25 adds an optional graphical desktop controller and strengthens
the client runtime around concurrent GPU execution and Pollard Kangaroo.

## Desktop interface

The optional Qt 6 interface controls the existing audited command-line client.
It does not communicate with the coordination API or maintain separate
assignment state.

- English is the default language, with Portuguese, French and Spanish also
  available.
- Light and black-and-gold OpenPuzzle themes are included.
- Official branding is embedded in the window, desktop launcher and rounded
  application icon.
- The status dashboard represents CUDA, OpenCL, CPU/KeyHunt and compatible
  primary slots independently.
- Starting BitCrack with concurrent CUDA and OpenCL is available as one simple
  execution mode with separate device selectors.
- Benchmark, self-test, doctor, update check, audit and Kangaroo installation
  are exposed as buttons.
- Command messages, automatically refreshed status and the private detached
  runtime log use separate views so refreshes do not replace user output.
- A potential solution raises a visible notice without reading or displaying
  private-key contents.

The interface can optionally enable a private systemd user service for the
selected search at computer startup. This setting is disabled by default and
does not start or stop the current execution when changed.

## Read-only status

`openpuzzle status` now uses a strictly local, read-only inspection path.
Status checks never upload progress or completion, remove assignment state or
calibrate a performance profile. Only the supervisor that owns a runtime slot
performs those operations.

This prevents frequent graphical status polling from racing a completed engine
and leaving the continuous CUDA/OpenCL supervisors waiting without engines.
Live validation confirmed that both slots completed their assignments, claimed
new assignments and continued running while the interface remained open.

## Kangaroo execution

- Kangaroo is explicitly restricted to one exclusive CUDA slot.
- Concurrent CPU or OpenCL flags are rejected before an assignment is claimed.
- The desktop selector exposes Kangaroo only for supported puzzles 140, 145,
  150, 155 and 160.
- Host memory is selected conservatively from the physical or container memory
  limit, leaving operating-system and client headroom.
- Checkpoint recovery remains deterministic and independent of GPU VRAM.
- Safe Stop is disabled in the interface for unbounded Kangaroo work; the
  controlled Stop action preserves any checkpoint created by the engine.

## Runtime resilience

Temporary process-identity or local-state observation gaps no longer cause the
monitor to cancel an active assignment. Runtime ownership, process identity and
slot isolation remain enforced.

Detached graphical launches close standard input and redirect both output
channels to a private user-only runtime log. Closing the interface does not
stop the client.

## Compatibility

- BitCrack continues to support CUDA and OpenCL.
- KeyHunt remains available as an independent CPU slot.
- Existing configuration, benchmark profiles and client identity are retained.
- The coordination protocol and server endpoints are unchanged.
- The Qt 6 interface remains optional; command-line-only builds are supported.

## Validation

The complete isolated suite contains 114 tests. It covers command construction,
multi-slot display, language and theme preferences, desktop installation,
autostart configuration, Kangaroo memory planning and exclusivity, runtime
identity gaps, solution notices and the read-only status contract.

Production validation also completed real puzzle 71 assignments on concurrent
CUDA and OpenCL slots and confirmed automatic transition to new assignments
while the interface polled status.

## Upgrade

Allow active assignments to finish with `openpuzzle safestop` before replacing
the installed package:

    sudo apt install ./OpenPuzzle-1.0.25-Linux-x86_64.deb

The desktop interface is optional and can be installed for the current user
without replacing the running command-line client.

# OpenPuzzle 1.0.22 — Solution review client safeguards

OpenPuzzle 1.0.22 connects the Linux client to the server-side solution
verification workflow and strengthens protection against duplicate engine
processes.

## Solution review control

When the coordination service reports that a puzzle is awaiting solution
verification, the client now stops the active engine and pauses further work
for that puzzle.

The client distinguishes between:

- a puzzle awaiting independent solution verification;
- a puzzle confirmed as solved;
- ordinary assignment completion or expiry.

The private key is never transmitted by this control flow. Local solution
exports remain protected on the machine that found the candidate.

## Safe monitoring

A temporary failure to load local execution state no longer causes the
continuous runtime to interpret the assignment as completed. Monitoring is
preserved and retried, preventing an unnecessary new claim or engine launch.

## Duplicate engine protection

Before starting an engine, OpenPuzzle checks the workspace process identity,
including the system boot identifier and Linux process start time.

If a matching live execution already exists in that workspace, the second
launch is rejected without overwriting the original process metadata.

## Test isolation

Routing command tests now use isolated HOME and XDG runtime directories. Test
execution cannot read, replace or remove production runtime state.

## Engine compatibility

This release preserves support for:

- BitCrack with CUDA;
- BitCrack with OpenCL and Rusticl;
- KeyHunt CPU range mode;
- PSCKangaroo for supported puzzles.

## Upgrade

Stop active work safely before upgrading:

```bash
openpuzzle safestop
openpuzzle status
```

Install the Debian package:

```bash
sudo apt install ./OpenPuzzle-1.0.22-Linux-x86_64.deb
```

Confirm the installed version:

```bash
openpuzzle --version
```

# OpenPuzzle 1.0.23 — Complete GPU and engine inventory

OpenPuzzle 1.0.23 improves the client capability information reported by
heartbeats. This release allows authenticated operator tooling to represent
CUDA, OpenCL and Pollard Kangaroo availability accurately.

## GPU inventory

The heartbeat now collects the complete backend-aware GPU inventory instead
of reporting only CUDA devices.

- CUDA devices remain reported with the `CUDA` backend.
- OpenCL devices are reported with the `OpenCL` backend.
- AMD GPUs exposed through Mesa Rusticl are included when the client is
  launched with the appropriate `--rusticl-enable` selector.
- Invalid inventory entries without a backend or device name are discarded.

A GPU available through more than one compute backend may appear once for
each backend. This is intentional because scheduling capability depends on
the backend as well as the physical device.

## Engine inventory

The heartbeat now advertises four engine/backend capabilities:

- BitCrack / CUDA
- BitCrack / OpenCL
- KeyHunt / CPU
- Kangaroo / CUDA

Kangaroo availability is determined through the existing validated
PSCKangaroo executable discovery. Machines without that executable report
the capability as not installed.

## Compatibility

- Client identity continues to use the existing UUID.
- The heartbeat remains compatible with the existing server contract.
- OpenPuzzle 1.0.22 clients remain accepted by the server.
- Assignment claiming, progress, completion and safestop behaviour are
  unchanged.
- No automatic startup is introduced.

## Validation

The GPU conversion has deterministic coverage for CUDA and OpenCL entries,
including invalid-entry filtering. The client heartbeat service also verifies
that the Kangaroo/CUDA capability is always present in the inventory.

The complete isolated test suite passed before release preparation.

## Upgrade

Install the Debian package normally:

    sudo apt install ./OpenPuzzle-1.0.23-Linux-x86_64.deb

Running assignments should be allowed to finish with `openpuzzle safestop`
before replacing the installed package.

# OpenPuzzle 1.0.29 — Private per-GPU dashboard telemetry

OpenPuzzle 1.0.29 extends the read-only GPU telemetry introduced in 1.0.28 to
the authenticated administrative node dashboard. Operators can inspect the
temperature and power of every safely matched active GPU without exposing
hardware sensor data through the public website or public network API.

## Optional heartbeat telemetry

Active clients include an optional point-in-time physical GPU snapshot in
their normal authenticated heartbeat. Each reading can contain:

- vendor and stable physical device identity;
- device index and PCI bus identifier when available;
- temperature and its sensor label;
- current power draw;
- configured power limit.

Missing tools or unsupported sensors remain absent rather than being reported
as zero. The heartbeat extension is optional, so clients up to 1.0.28 remain
compatible with the coordination server.

## Physical-device deduplication

Telemetry describes physical GPUs, not backend capabilities. One NVIDIA card
visible through both CUDA and OpenCL is reported once. AMD readings retain the
kernel DRM and PCI identity required for conservative matching.

During concurrent and multi-GPU execution, exactly one supervisor owns the
shared telemetry snapshot. Supervised workers omit the optional field and
cannot duplicate or erase the machine-level readings.

## Private per-GPU presentation

The authenticated Admin Nodes dashboard displays temperature, power draw and
power limit directly after the speed of the matching active assignment.

Dynamic CUDA slots have an exact relationship: `cuda-N` maps to NVIDIA device
N. A legacy `primary` CUDA slot maps only when the client reports CUDA as its
sole active GPU backend and one compatible NVIDIA reading exists.

OpenCL and generic slots are associated only when exactly one unused current
physical reading remains. Ambiguous readings are omitted instead of being
assigned to the wrong GPU.

Global temperature and power summary cards are intentionally omitted. The
per-assignment reading remains the authoritative operational view.

## Privacy boundary

GPU telemetry is restricted to the authenticated administrative dashboard.
The public homepage, public JavaScript and `/api/network/status/` do not expose:

- temperatures;
- power draw or power limits;
- telemetry device counts;
- physical GPU identifiers or PCI addresses.

The server treats telemetry storage as optional. A missing migration or a
temporary sensor-table query failure cannot make the node dashboard or public
network API unavailable.

## Safety and compatibility

Telemetry remains observational. OpenPuzzle does not change clocks, fan
control or power limits. The disabled-by-default thermal policy from 1.0.28 is
unchanged and remains the only component that can request an orderly stop at a
critical temperature when explicitly configured with `--stop-on-critical`.

BitCrack CUDA and OpenCL, KeyHunt CPU, optional PSCKangaroo routing,
multi-CUDA supervision, assignment fairness, client identity, the desktop
interface and the updater contract are unchanged.

## Validation

The complete client suite contains 132 automated tests. Coverage includes:

- optional heartbeat serialization and backward compatibility;
- NVIDIA and AMD physical-device snapshots;
- duplicate physical-GPU suppression across CUDA and OpenCL;
- single-supervisor ownership during multi-GPU execution;
- dynamic CUDA and legacy primary-slot association contracts;
- ambiguous-device rejection;
- administrative-only presentation and public API privacy;
- existing runtime, thermal safety and release contracts.

Production validation confirmed a live CUDA `primary` assignment with its
temperature, power draw and power limit displayed immediately after the GPU
speed. The public network-status API remained telemetry-free.

## Upgrade and rig validation

Allow active assignments to finish before replacing an installed client:

    openpuzzle safestop

Install the Debian package:

    sudo apt install ./OpenPuzzle-1.0.29-Linux-x86_64.deb

Confirm the installation and local sensors:

    openpuzzle --version
    openpuzzle doctor --offline
    openpuzzle thermal

On a multi-GPU rig, start all usable CUDA devices with:

    openpuzzle run 71 \
      --engine bitcrack \
      --backend cuda \
      --devices all

Verify that Admin Nodes places one current temperature and power reading after
the speed of every active `cuda-N` assignment.

The portable updater package retains the established identity:

    OpenPuzzle-1.0.29-portable-XXXXXXXX.deb

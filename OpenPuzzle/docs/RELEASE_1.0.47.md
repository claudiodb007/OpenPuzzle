# OpenPuzzle 1.0.47 — GPU power and fan controls on the desktop

OpenPuzzle 1.0.47 adds a GPU control card to the Linux desktop. NVIDIA boards use
NVML and AMD boards use amdgpu hwmon for supported power limits and fan controls.
The card selects a physical GPU by vendor and PCI address, with NVIDIA UUID
verification, independently of CUDA/OpenCL profile indexes.

## Power and fans

Power limits use watts on Linux and stay within the driver's supported range
and the supported default maximum. Fans support driver Auto and a fixed Manual
percentage where exposed by the board. Manual requires at least 30% and a valid
GPU temperature below 80 C when applied. It is a fixed target, not an OpenPuzzle
temperature curve. Existing thermal search protection remains separate.

Each Apply confirmation shows the selected board and requested setting. The
helper checks identity and support again before writing, reads back the result,
and attempts rollback after an inconsistent or failed change. Failed rollback
is reported explicitly. Opening or refreshing the panel never reapplies tuning.
Driver support and permissions vary by board; unsupported writes are reported.

## Installed permissions and readable dialogs

The Linux packages include the root-owned `openpuzzle-gpu-control` helper and a
polkit action restricted to that executable. Each Apply requests administrator
authentication while the desktop panel stays unprivileged. A graphical polkit
agent and `/usr/bin/pkexec` must be present in the desktop session. Installation
does not tune a GPU, start a search or enable a background tuning service.

Qt confirmations now have opaque backgrounds and readable buttons in light and
dark themes. Portable TGZ extraction does not itself authorize hardware writes;
the included helper-installation script is available for explicit installation.
See [GPU controls](GPU_CONTROLS.md) for commands, limitations and driver details.

## Validation and Windows development status

Before preparing the versioned release, all 176 Linux suites passed, including
real staged-install and DEB/TGZ content checks, helper ownership/permissions,
restricted polkit scope, GPU transaction fixtures, UI cancellation and rendered
light/dark confirmations. The complete Linux suite is rerun on this versioned
source before release assets are generated. NVIDIA and AMD controls were also
confirmed manually on Linux.

The source also includes the experimental native Windows panel: four shared
languages, automatic mandatory coordinator, Benchmark, Safe Stop, buffered
scrolling, verified physical GPU telemetry, NVIDIA power/fan controls and live
search speed. The Windows development package passed 41 native MSVC suites and
local synthetic KeyHunt/CUDA/OpenCL assignments. NVIDIA RTX 3080 CUDA search,
300 W power limiting, 75% manual fans, return to driver Auto and displayed
speed were confirmed manually. These are development validations; this release
ships Linux packages only. A separate
[Windows 11 x64 public test prerelease](https://github.com/claudiodb007/OpenPuzzle/releases/tag/windows-test-20261017-1)
includes a per-user PowerShell installer and custom shortcut icons. Native
packaging checks covered private DACLs, shortcut icon resolution, ZIP roundtrip,
installation/removal preservation and reinstallation. The installed panel was
also reviewed manually. Those Windows test executables retain their tested
identity and are not rebuilt or relabelled by this Linux release. AMD Windows
hardware validation and an MSI/Setup.exe installer remain pending. No AMD Windows
support is inferred from adapter compilation alone.

## Packages and upgrade

The release assets are:

- `OpenPuzzle-1.0.47-Linux-x86_64.deb`
- `OpenPuzzle-1.0.47-Linux-x86_64.tar.gz`
- `OpenPuzzle-1.0.47-portable-XXXXXXXX.deb` (same DEB bytes; checksum prefix)
- `OpenPuzzle-1.0.47-source.tar.gz`
- Release manifest and SHA-256 checksum files.

Let active OpenPuzzle assignments finish before replacing the installed client:

```sh
openpuzzle safestop
```

After the relevant runtimes are idle, verify the published checksums and install:

```sh
sudo apt install ./OpenPuzzle-1.0.47-Linux-x86_64.deb
openpuzzle --version
openpuzzle doctor --offline
openpuzzle status
```

Start searches explicitly with the established backend/device arguments.
An upgrade does not request new assignments or automatically apply GPU tuning.
Existing client configuration, measured profiles and local solution data are
not included in the release assets.

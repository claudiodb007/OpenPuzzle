# OpenPuzzle 1.0.48 — Automatic GPU benchmarks from the desktop

This candidate integrates the automatic desktop benchmark and first-profile
work validated on Linux and Windows. Linux packages require a fresh release
build and 178 native suites plus 24 isolated platform suites before use.

## Linux desktop

Benchmark now opens a confirmation dialog for CUDA or OpenCL and the selected
device. After confirmation it performs a real automatic measurement, streams
progress and saves the selected measured profile. Stop cancels the owned
benchmark; failure or cancellation preserves the previous profile. Stop active
searches before measuring. A successful benchmark updates the selected GPU's
profile; it does not change power, clocks or fans.

The shared tuning matrix, stability checks, 60-second finalist and 3% planning
margin are retained. GPU power/fan controls and the authenticated helper from
1.0.47 remain included. Manual Ubuntu benchmark success was reported separately
from automated synthetic tests; no unreported speed or hardware is inferred.

## Windows source and separate test package

The source includes the first-profile and automatic CUDA/OpenCL benchmark
changes from commit 85946b2bcc76dc143dd69bc7b3a40a9ccdcf0752. That preview passed
43 MSVC fixture suites; the tester separately confirmed operation on Windows 10
and 11. The updated Windows installer is a separate experimental package with
its own identity and validation. Reused Windows binaries retain their original
version string; the Linux version does not relabel those binaries.

## Candidate artifacts

- `OpenPuzzle-1.0.48-Linux-x86_64.deb`
- `OpenPuzzle-1.0.48-Linux-x86_64.tar.gz`
- `OpenPuzzle-1.0.48-portable-XXXXXXXX.deb` (identical DEB; checksum prefix)
- `OpenPuzzle-1.0.48-source.tar.gz`
- Release manifest and SHA-256 files.

Preparation does not install, publish, measure GPUs or request production work.
Review the candidate report before installing. Let active assignments finish
using Safe Stop before replacing the installed client. Verify the checksums,
then install the DEB explicitly. Configuration, measured profiles and solution
files are never included in distribution archives. GPU settings are changed
only by an explicit Apply in the existing control panel.

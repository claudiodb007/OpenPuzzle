# OpenPuzzle Vanity engine source

Exact corresponding source for the native binary packaged in OpenPuzzle 1.0.53.
SOURCE_MANIFEST.json and BUILD_MANIFEST.json record source and binary hashes.
Build with CUDA Toolkit 12.x and a supported host compiler:
`NVCC=/path/to/nvcc CXX=/path/to/g++ ./build.sh`.
CPU units retain -fno-strict-aliasing. Existing CUDA architectures and kernels
are unchanged. offline.cpp selects blocking synchronization for its owned CUDA
context and verifies the result. GPU errors remain fatal.

This exact binary passed known-address/WIF checks on five RTX 3080 cards and a
30-second simultaneous local measurement. Other compiled families have not all
been tested on hardware. GPL-3.0: see LICENSE. Private validation files are not
part of the release archive.

## Native Windows build

The source now has a separate Windows x64 build target. The manifests above
describe the released Linux binary. On Windows, keep the full repository
checkout so the engine can use the client's Windows file protection.
From a Visual Studio 2022 development shell with CUDA Toolkit 12.8 and CMake
3.24 or later:

```powershell
cmake -S OpenPuzzle-Vanity -B build-vanity-win -G "Visual Studio 17 2022" -A x64
cmake --build build-vanity-win --config Release
ctest --test-dir build-vanity-win -C Release --output-on-failure
```

The tests check engine identity, Unicode target paths, exclusive output
creation and the parent process and Job Object checks. A search requires an
owned Job Object with kill-on-close. The engine accepts a single CUDA device
exposed by its supervisor.

OpenPuzzle 1.0.61 ships the native engine for Windows x64. The Windows desktop
uses the measured CUDA device profile to launch the engine for Puzzle 71.
The client verifies coverage against the assigned range before confirming
completion and uses the same assignment when recovering a valid lease.
The build links with an 8 MiB stack reserve for CUDA initialization.

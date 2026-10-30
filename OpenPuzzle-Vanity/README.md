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

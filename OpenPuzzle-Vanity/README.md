# OpenPuzzle Vanity engine source

Exact corresponding source for the separate `vanity-offline` binary in OpenPuzzle
1.0.51. SOURCE_MANIFEST.json records the unchanged engine source. BUILD_MANIFEST.json
records architectures, compiler options and binary SHA-256. GPL-3.0: see LICENSE.

Build using CUDA Toolkit 12.x and a supported host C++ compiler:
```
NVCC=/path/to/cuda-12/bin/nvcc CXX=/path/to/g++ ./build.sh
```
Output: build/vanity-offline. Native targets: sm_50,52,60,61,70,75,80,86,89,90;
compute_50 PTX permits forward JIT where the driver supports it. CPU C++ units
use -fno-strict-aliasing for the legacy hash implementation. CUDA source is unchanged.
The concrete architecture list is recorded in BUILD_MANIFEST.json.

Real short acceptance passed on RTX 4070 SUPER and five RTX 3080 cards individually.
Other compiled families are not thereby hardware-validated. OpenPuzzle and this
engine communicate through a process protocol and form a packaged aggregate.

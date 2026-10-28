# OpenPuzzle Vanity engine source

This directory contains the exact corresponding source for the separately
executed `vanity-offline` binary distributed by OpenPuzzle 1.0.49.
`SOURCE_MANIFEST.json` records every validated source SHA-256. Build with a
CUDA toolkit by running `./build.sh`; the output is `build/vanity-offline`.

The engine is licensed under GNU GPL version 3; see `LICENSE`. OpenPuzzle and
the engine communicate through a process protocol and are packaged together
as an aggregate, not linked into one executable.

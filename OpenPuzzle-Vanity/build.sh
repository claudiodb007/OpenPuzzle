#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
mkdir -p -- "$root/build"
exec nvcc -O3 -std=c++17 -Xcompiler=-pthread -I"$root/source" \
+  "$root/source/Base58.cpp" \
+  "$root/source/Bech32.cpp" \
+  "$root/source/GPU/GPUEngine.cu" \
+  "$root/source/GPU/GPUGenerate.cpp" \
+  "$root/source/Int.cpp" \
+  "$root/source/IntGroup.cpp" \
+  "$root/source/IntMod.cpp" \
+  "$root/source/Point.cpp" \
+  "$root/source/Random.cpp" \
+  "$root/source/SECP256K1.cpp" \
+  "$root/source/Timer.cpp" \
+  "$root/source/Wildcard.cpp" \
+  "$root/source/hash/ripemd160.cpp" \
+  "$root/source/hash/ripemd160_sse.cpp" \
+  "$root/source/hash/sha256.cpp" \
+  "$root/source/hash/sha256_sse.cpp" \
+  "$root/source/hash/sha512.cpp" \
+  "$root/source/offline.cpp" \
+  -o "$root/build/vanity-offline"

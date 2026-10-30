#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
mkdir -p -- "$root/build"
flags=(-O3 -std=c++17 -include cstdint -ccbin "${CXX:-g++}"
  -Xcompiler=-pthread -Xcompiler=-mssse3 -Xcompiler=-msse4.1 -I"$root/source"
  -gencode=arch=compute_50,code=sm_50
  -gencode=arch=compute_52,code=sm_52
  -gencode=arch=compute_60,code=sm_60
  -gencode=arch=compute_61,code=sm_61
  -gencode=arch=compute_70,code=sm_70
  -gencode=arch=compute_75,code=sm_75
  -gencode=arch=compute_80,code=sm_80
  -gencode=arch=compute_86,code=sm_86
  -gencode=arch=compute_89,code=sm_89
  -gencode=arch=compute_90,code=sm_90
  -gencode=arch=compute_50,code=compute_50)
"${NVCC:-nvcc}" "${flags[@]}" -Xcompiler=-fno-strict-aliasing -c "$root/source/Base58.cpp" -o "$root/build/Base58.cpp.o"
"${NVCC:-nvcc}" "${flags[@]}" -Xcompiler=-fno-strict-aliasing -c "$root/source/Bech32.cpp" -o "$root/build/Bech32.cpp.o"
"${NVCC:-nvcc}" "${flags[@]}" -Xcompiler=-fno-strict-aliasing -c "$root/source/GPU/GPUGenerate.cpp" -o "$root/build/GPU_GPUGenerate.cpp.o"
"${NVCC:-nvcc}" "${flags[@]}" -Xcompiler=-fno-strict-aliasing -c "$root/source/Int.cpp" -o "$root/build/Int.cpp.o"
"${NVCC:-nvcc}" "${flags[@]}" -Xcompiler=-fno-strict-aliasing -c "$root/source/IntGroup.cpp" -o "$root/build/IntGroup.cpp.o"
"${NVCC:-nvcc}" "${flags[@]}" -Xcompiler=-fno-strict-aliasing -c "$root/source/IntMod.cpp" -o "$root/build/IntMod.cpp.o"
"${NVCC:-nvcc}" "${flags[@]}" -Xcompiler=-fno-strict-aliasing -c "$root/source/Point.cpp" -o "$root/build/Point.cpp.o"
"${NVCC:-nvcc}" "${flags[@]}" -Xcompiler=-fno-strict-aliasing -c "$root/source/Random.cpp" -o "$root/build/Random.cpp.o"
"${NVCC:-nvcc}" "${flags[@]}" -Xcompiler=-fno-strict-aliasing -c "$root/source/SECP256K1.cpp" -o "$root/build/SECP256K1.cpp.o"
"${NVCC:-nvcc}" "${flags[@]}" -Xcompiler=-fno-strict-aliasing -c "$root/source/Timer.cpp" -o "$root/build/Timer.cpp.o"
"${NVCC:-nvcc}" "${flags[@]}" -Xcompiler=-fno-strict-aliasing -c "$root/source/Wildcard.cpp" -o "$root/build/Wildcard.cpp.o"
"${NVCC:-nvcc}" "${flags[@]}" -Xcompiler=-fno-strict-aliasing -c "$root/source/hash/ripemd160.cpp" -o "$root/build/hash_ripemd160.cpp.o"
"${NVCC:-nvcc}" "${flags[@]}" -Xcompiler=-fno-strict-aliasing -c "$root/source/hash/ripemd160_sse.cpp" -o "$root/build/hash_ripemd160_sse.cpp.o"
"${NVCC:-nvcc}" "${flags[@]}" -Xcompiler=-fno-strict-aliasing -c "$root/source/hash/sha256.cpp" -o "$root/build/hash_sha256.cpp.o"
"${NVCC:-nvcc}" "${flags[@]}" -Xcompiler=-fno-strict-aliasing -c "$root/source/hash/sha256_sse.cpp" -o "$root/build/hash_sha256_sse.cpp.o"
"${NVCC:-nvcc}" "${flags[@]}" -Xcompiler=-fno-strict-aliasing -c "$root/source/hash/sha512.cpp" -o "$root/build/hash_sha512.cpp.o"
"${NVCC:-nvcc}" "${flags[@]}" -Xcompiler=-fno-strict-aliasing -c "$root/source/offline.cpp" -o "$root/build/offline.cpp.o"
"${NVCC:-nvcc}" "${flags[@]}" -c "$root/source/GPU/GPUEngine.cu" -o "$root/build/GPU_GPUEngine.cu.o"
exec "${NVCC:-nvcc}" -ccbin "${CXX:-g++}" -Xcompiler=-pthread \
  "$root/build/Base58.cpp.o" \
  "$root/build/Bech32.cpp.o" \
  "$root/build/GPU_GPUGenerate.cpp.o" \
  "$root/build/Int.cpp.o" \
  "$root/build/IntGroup.cpp.o" \
  "$root/build/IntMod.cpp.o" \
  "$root/build/Point.cpp.o" \
  "$root/build/Random.cpp.o" \
  "$root/build/SECP256K1.cpp.o" \
  "$root/build/Timer.cpp.o" \
  "$root/build/Wildcard.cpp.o" \
  "$root/build/hash_ripemd160.cpp.o" \
  "$root/build/hash_ripemd160_sse.cpp.o" \
  "$root/build/hash_sha256.cpp.o" \
  "$root/build/hash_sha256_sse.cpp.o" \
  "$root/build/hash_sha512.cpp.o" \
  "$root/build/offline.cpp.o" \
  "$root/build/GPU_GPUEngine.cu.o" \
  -o "$root/build/vanity-offline"

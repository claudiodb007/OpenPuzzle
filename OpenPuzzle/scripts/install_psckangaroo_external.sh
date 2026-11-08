#!/usr/bin/env bash
set -Eeuo pipefail

export LC_ALL=C

UPSTREAM_URL="https://github.com/pscamillo/PSCKangaroo.git"
UPSTREAM_COMMIT="021e997a2c9de28d86e3d8db3a5f65188653a80a"
UPSTREAM_TREE="61533742ddcde64df0d993093317b570c68026a7"
PATCH_ID="explicit-seed-v1"
PATCH_SHA256="59092a431b94eacebc66a46afc202a84d1b21e162e339bfcaeb9ee25b3c2a03e"
PATCHED_RCK_SHA256="23a49400346c83f65844290ddc6452576a23e480281c42189609e0e54e24e13e"
PATCHED_README_SHA256="a2f33d35cd571523bc20f2089ae51dc2972ee9b2a21f77d546769261830cbd8d"
CHECKPOINT_PATCH_ID="bitmap-checkpoint-v1"
CHECKPOINT_PATCH_SHA256="94f9c4a4aa7242d5e943eef2342251ceae5cdfa0c32a18cff0b7f7082d553e14"
PATCHED_TAMESTORE_SHA256="dd366a19728d167f9985a52d62de3417f06a2911747bd0bb6d0f9f11db2a98ff"
ARCHITECTURES="sm_60 sm_61 sm_70 sm_75 sm_80 sm_86 sm_89 sm_90 compute_89"
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
patch_file="$script_dir/patches/psckangaroo-explicit-seed-v1.patch"
checkpoint_patch_file="$script_dir/patches/psckangaroo-bitmap-checkpoint-v1.patch"

usage() {
  cat <<'EOF'
OpenPuzzle PSCKangaroo installer
--------------------------------
Usage:
  openpuzzle engine install psckangaroo [--force]

This explicit command downloads pinned GPLv3 source, verifies its Git commit
and tree, applies hash-verified walk-seed and bitmap-checkpoint patches,
builds it locally with the installed NVIDIA CUDA toolkit, and places
the executable in the current user's OpenPuzzle engine directory.

The PSCKangaroo executable is not downloaded from or redistributed by the
OpenPuzzle package. The installer never starts the engine or performs GPU work.
EOF
}

die() {
  printf 'ERROR: %s\n' "$*" >&2
  exit 1
}

force=0
for argument in "$@"; do
  case "$argument" in
    --force) force=1 ;;
    -h|--help) usage; exit 0 ;;
    *) die "unknown installer option: $argument" ;;
  esac
done

[[ "$(uname -s)" == "Linux" ]] || die "only Linux is supported"
[[ "$(uname -m)" == "x86_64" ]] || die "only x86_64 is supported"

for command in git nvcc g++ sha256sum readelf file install mktemp awk dirname strip; do
  command -v "$command" >/dev/null 2>&1 || die "required command not found: $command"
done

if [[ -n "${XDG_DATA_HOME:-}" ]]; then
  data_home="$XDG_DATA_HOME"
elif [[ -n "${HOME:-}" ]]; then
  data_home="$HOME/.local/share"
else
  die "HOME and XDG_DATA_HOME are unavailable"
fi

engine_dir="$data_home/OpenPuzzle/engines"
target="$engine_dir/psckangaroo"
manifest="$engine_dir/psckangaroo.openpuzzle-manifest"

[[ ! -L "$engine_dir" ]] || die "engine directory must not be a symbolic link"
[[ ! -L "$target" ]] || die "target executable must not be a symbolic link"

if [[ -x "$target" && "$force" -eq 0 ]]; then
  printf 'PSCKangaroo is already installed at %s\n' "$target"
  printf 'Use --force to rebuild the pinned version.\n'
  exit 0
fi

work="$(mktemp -d "${TMPDIR:-/tmp}/openpuzzle-psckangaroo.XXXXXXXX")"
cleanup() {
  rm -rf -- "$work"
}
trap cleanup EXIT

printf '#include <zlib.h>
int main() { return crc32(0, nullptr, 0); }
' |
  g++ -std=c++17 -x c++ - -o "$work/zlib-link-check" -lz ||
  die "building the bitmap candidate requires zlib development headers and library (zlib1g-dev on Ubuntu)"

source_dir="$work/PSCKangaroo"

printf 'OpenPuzzle PSCKangaroo local installation\n'
printf '%s\n' '-----------------------------------------'
printf 'Source.............. %s\n' "$UPSTREAM_URL"
printf 'Commit.............. %s\n' "$UPSTREAM_COMMIT"
printf 'Compatibility patch. %s (%s)\n' "$PATCH_ID" "$PATCH_SHA256"
printf 'Checkpoint storage.. %s (%s)\n' "$CHECKPOINT_PATCH_ID" "$CHECKPOINT_PATCH_SHA256"
printf 'Destination......... %s\n' "$target"
printf 'CUDA runtime........ static local link\n'
printf 'GPU execution....... none\n\n'

git clone --quiet --no-checkout "$UPSTREAM_URL" "$source_dir"
git -C "$source_dir" checkout --quiet --detach "$UPSTREAM_COMMIT"

actual_commit="$(git -C "$source_dir" rev-parse HEAD)"
actual_tree="$(git -C "$source_dir" rev-parse 'HEAD^{tree}')"
[[ "$actual_commit" == "$UPSTREAM_COMMIT" ]] || die "pinned commit verification failed"
[[ "$actual_tree" == "$UPSTREAM_TREE" ]] || die "pinned tree verification failed"
[[ -z "$(git -C "$source_dir" status --porcelain --untracked-files=no)" ]] || \
  die "pinned source worktree is not clean"

[[ -f "$patch_file" && ! -L "$patch_file" ]] || die "compatibility patch is missing or unsafe"
actual_patch_sha="$(sha256sum "$patch_file" | awk '{print $1}')"
[[ "$actual_patch_sha" == "$PATCH_SHA256" ]] || die "compatibility patch hash verification failed"
git -C "$source_dir" apply --check "$patch_file" || die "compatibility patch preflight failed"
git -C "$source_dir" apply "$patch_file" || die "compatibility patch application failed"

[[ -f "$checkpoint_patch_file" && ! -L "$checkpoint_patch_file" ]] || die "checkpoint patch is missing or unsafe"
actual_checkpoint_patch_sha="$(sha256sum "$checkpoint_patch_file" | awk '{print $1}')"
[[ "$actual_checkpoint_patch_sha" == "$CHECKPOINT_PATCH_SHA256" ]] || die "checkpoint patch hash verification failed"
git -C "$source_dir" apply --check "$checkpoint_patch_file" || die "checkpoint patch preflight failed"
git -C "$source_dir" apply "$checkpoint_patch_file" || die "checkpoint patch application failed"
git -C "$source_dir" diff --check || die "patched source whitespace validation failed"
patched_paths="$(git -C "$source_dir" diff --name-only -- | sort)"
expected_patched_paths=$'RCKangaroo_hunt_v2.cpp\nREADME.md\nTameStore.h'
[[ "$patched_paths" == "$expected_patched_paths" ]] || die "compatibility patch changed unexpected files"
[[ "$(sha256sum "$source_dir/RCKangaroo_hunt_v2.cpp" | awk '{print $1}')" == "$PATCHED_RCK_SHA256" ]] || \
  die "patched RCKangaroo source hash verification failed"
[[ "$(sha256sum "$source_dir/README.md" | awk '{print $1}')" == "$PATCHED_README_SHA256" ]] || \
  die "patched README hash verification failed"
git -C "$source_dir" apply --reverse --check "$patch_file" || die "applied patch reverse verification failed"
[[ "$(sha256sum "$source_dir/TameStore.h" | awk '{print $1}')" == "$PATCHED_TAMESTORE_SHA256" ]] || \
  die "patched checkpoint source hash verification failed"
git -C "$source_dir" apply --reverse --check "$checkpoint_patch_file" || die "checkpoint patch reverse verification failed"

common=(
  -O3 -march=x86-64 -mtune=generic -pthread
  -DV46_ENABLE=0 -DV46_EXPLORER_PCT=20 -DV46_EXPLORER_SHIFT=2
  -DV45_OCCUPANCY=1 -DV45_TABLE_BITS=33
  -DV45_PNT_GROUP_CNT=48 -DV45_STEP_CNT=1000
)

gpu_arch=(
  -gencode=arch=compute_60,code=sm_60
  -gencode=arch=compute_61,code=sm_61
  -gencode=arch=compute_70,code=sm_70
  -gencode=arch=compute_75,code=sm_75
  -gencode=arch=compute_80,code=sm_80
  -gencode=arch=compute_86,code=sm_86
  -gencode=arch=compute_89,code=sm_89
  -gencode=arch=compute_90,code=sm_90
  -gencode=arch=compute_89,code=compute_89
)

pushd "$source_dir" >/dev/null
g++ -std=c++17 "${common[@]}" -I/usr/include -c RCKangaroo_hunt_v2.cpp -o RCKangaroo_hunt_v2.o
g++ "${common[@]}" -c Ec.cpp -o Ec.o
g++ "${common[@]}" -I/usr/include -c GpuKang.cpp -o GpuKang.o
nvcc -O3 "${gpu_arch[@]}" \
  -DV46_ENABLE=0 -DV46_EXPLORER_PCT=20 -DV46_EXPLORER_SHIFT=2 \
  -DV45_OCCUPANCY=1 -DV45_TABLE_BITS=33 \
  -DV45_PNT_GROUP_CNT=48 -DV45_STEP_CNT=1000 \
  -Xcompiler -O3,-march=x86-64,-mtune=generic,-pthread \
  --ptxas-options=-v --cudart=static \
  -c RCGpuCore.cu -o RCGpuCore.o
g++ "${common[@]}" -c utils.cpp -o utils.o
nvcc -O3 "${gpu_arch[@]}" \
  -DV46_ENABLE=0 -DV46_EXPLORER_PCT=20 -DV46_EXPLORER_SHIFT=2 \
  -DV45_OCCUPANCY=1 -DV45_TABLE_BITS=33 \
  -DV45_PNT_GROUP_CNT=48 -DV45_STEP_CNT=1000 \
  -Xcompiler -O3,-march=x86-64,-mtune=generic,-pthread \
  --ptxas-options=-v --cudart=static \
  -o psckangaroo \
  RCKangaroo_hunt_v2.o Ec.o GpuKang.o RCGpuCore.o utils.o \
  -lcuda -lpthread -lz
popd >/dev/null

[[ -f "$source_dir/psckangaroo" ]] || die "build did not produce psckangaroo"

strip --strip-all "$source_dir/psckangaroo"
if readelf -SW "$source_dir/psckangaroo" | grep -Eq '] \.symtab|] \.strtab'; then
  die "stripped executable still contains non-runtime symbol tables"
fi

if readelf -d "$source_dir/psckangaroo" | grep -q 'libcudart\.so'; then
  die "built executable unexpectedly requires shared libcudart"
fi

mkdir -p -- "$engine_dir"
chmod 700 -- "$engine_dir"

temporary_target="$engine_dir/.psckangaroo.$$"
temporary_manifest="$engine_dir/.psckangaroo.openpuzzle-manifest.$$"
install -m 700 "$source_dir/psckangaroo" "$temporary_target"

binary_sha="$(sha256sum "$temporary_target" | awk '{print $1}')"
cat >"$temporary_manifest" <<EOF
schema=openpuzzle-external-engine-v2
engine=psckangaroo
source_url=$UPSTREAM_URL
source_commit=$UPSTREAM_COMMIT
source_tree=$UPSTREAM_TREE
patch_id=$PATCH_ID
patch_sha256=$PATCH_SHA256
patched_rck_sha256=$PATCHED_RCK_SHA256
patched_readme_sha256=$PATCHED_README_SHA256
checkpoint_storage=bitmap-blocks-v1
checkpoint_format=OPZKCP1-v4
checkpoint_patch_id=$CHECKPOINT_PATCH_ID
checkpoint_patch_sha256=$CHECKPOINT_PATCH_SHA256
patched_tamestore_sha256=$PATCHED_TAMESTORE_SHA256
binary_sha256=$binary_sha
architectures=$ARCHITECTURES
cuda_runtime=static-local-build
binary_symbols=stripped
EOF
chmod 600 "$temporary_manifest"

mv -f -- "$temporary_target" "$target"
mv -f -- "$temporary_manifest" "$manifest"

printf '\nPSCKangaroo installation complete\n'
printf 'Executable.......... %s\n' "$target"
printf 'SHA-256............ %s\n' "$binary_sha"
printf 'Manifest............ %s\n' "$manifest"
printf 'Binary symbols...... stripped\n'
printf 'GPU execution....... none\n'

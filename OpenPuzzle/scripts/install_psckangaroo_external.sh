#!/usr/bin/env bash
set -Eeuo pipefail

export LC_ALL=C

UPSTREAM_URL="https://github.com/pscamillo/PSCKangaroo.git"
UPSTREAM_COMMIT="021e997a2c9de28d86e3d8db3a5f65188653a80a"
UPSTREAM_TREE="61533742ddcde64df0d993093317b570c68026a7"
ARCHITECTURES="sm_60 sm_61 sm_70 sm_75 sm_80 sm_86 sm_89 sm_90 compute_89"

usage() {
  cat <<'EOF'
OpenPuzzle PSCKangaroo installer
--------------------------------
Usage:
  openpuzzle engine install psckangaroo [--force]

This explicit command downloads pinned GPLv3 source, verifies its Git commit
and tree, builds it locally with the installed NVIDIA CUDA toolkit, and places
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

for command in git nvcc g++ sha256sum readelf file install mktemp; do
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

source_dir="$work/PSCKangaroo"

printf 'OpenPuzzle PSCKangaroo local installation\n'
printf '%s\n' '-----------------------------------------'
printf 'Source.............. %s\n' "$UPSTREAM_URL"
printf 'Commit.............. %s\n' "$UPSTREAM_COMMIT"
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
g++ "${common[@]}" -I/usr/include -c RCKangaroo_hunt_v2.cpp -o RCKangaroo_hunt_v2.o
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
  -lcuda -lpthread
popd >/dev/null

[[ -f "$source_dir/psckangaroo" ]] || die "build did not produce psckangaroo"
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
schema=openpuzzle-external-engine-v1
engine=psckangaroo
source_url=$UPSTREAM_URL
source_commit=$UPSTREAM_COMMIT
source_tree=$UPSTREAM_TREE
binary_sha256=$binary_sha
architectures=$ARCHITECTURES
cuda_runtime=static-local-build
EOF
chmod 600 "$temporary_manifest"

mv -f -- "$temporary_target" "$target"
mv -f -- "$temporary_manifest" "$manifest"

printf '\nPSCKangaroo installation complete\n'
printf 'Executable.......... %s\n' "$target"
printf 'SHA-256............ %s\n' "$binary_sha"
printf 'Manifest............ %s\n' "$manifest"
printf 'GPU execution....... none\n'

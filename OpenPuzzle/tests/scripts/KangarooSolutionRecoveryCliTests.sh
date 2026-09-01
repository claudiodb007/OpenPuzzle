#!/usr/bin/env bash
set -Eeuo pipefail

[[ $# -eq 1 ]] || {
  echo "OpenPuzzle binary path is required" >&2
  exit 64
}

openpuzzle_bin="$1"
[[ -x "$openpuzzle_bin" ]] || {
  echo "OpenPuzzle binary is not executable: $openpuzzle_bin" >&2
  exit 65
}

test_root="$(mktemp -d "${TMPDIR:-/tmp}/openpuzzle-kangaroo-solution.XXXXXX")"
test_home="$test_root/home"
bin_dir="$test_root/bin"
assignment_id="41414141-4141-4141-8141-414141414141"
client_id="42424242-4242-4242-8242-424242424242"
workspace="$test_home/.local/share/OpenPuzzle/assignments/$assignment_id"
state_file="$test_home/.local/share/OpenPuzzle/client.state"
curl_capture="$test_root/curl-arguments.txt"
cli_output="$test_root/openpuzzle-output.txt"
private_key="0000000000000000000000000000000000000000000000000000000180000001"
wallet_file="$test_home/OpenPuzzle-Solutions/Puzzle-140/$assignment_id/wallet-import.txt"
notice_file="$test_home/OpenPuzzle-Solutions/KEY-FOUND-Puzzle-140-$assignment_id.txt"

cleanup() {
  if [[ -n "$test_root" && -d "$test_root" ]]; then
    rm -rf -- "$test_root"
  fi
}
trap cleanup EXIT

install -d -m 700 "$workspace" "$bin_dir"

cat > "$bin_dir/curl" <<'SH'
#!/bin/sh
printf '%s\n' "$*" >> "$OPENPUZZLE_FAKE_CURL_CAPTURE"
exit 0
SH
chmod 700 "$bin_dir/curl"

printf 'PRIVATE KEY: %s\n' "$private_key" > "$workspace/RESULTS.TXT"
chmod 600 "$workspace/RESULTS.TXT"

cat > "$state_file" <<STATE
active=1
puzzle=140
range_id=9002
pid=999999
boot_id=
process_start_time=0
device=0
blocks=0
threads=0
points=0
profile_managed=0
assignment_id=$assignment_id
client_id=$client_id
target=synthetic-address
public_key=031f6a332d3c5c4f2de2378c012f429cd109ba07d69690c6c701b6bb87860d6640
start=180000000
end=1800000FF
engine=Kangaroo
backend=CUDA
gpu_name=Synthetic GPU
workspace=$workspace
command=command-must-never-run
STATE
chmod 600 "$state_file"

env \
  HOME="$test_home" \
  PATH="$bin_dir:$PATH" \
  OPENPUZZLE_FAKE_CURL_CAPTURE="$curl_capture" \
  "$openpuzzle_bin" run 140 --server https://server.test \
  > "$cli_output" 2>&1

[[ -f "$state_file" ]]
[[ -f "$workspace/RESULTS.TXT" ]]
[[ -f "$wallet_file" ]]
[[ -f "$notice_file" ]]
[[ "$(stat -c '%a' "$wallet_file")" == "600" ]]
[[ "$(stat -c '%a' "$notice_file")" == "600" ]]

grep -Fq 'Recovering protected solution...' "$cli_output"
grep -Fq 'PRIVATE KEY FOUND - ACTION REQUIRED' "$cli_output"
grep -Fq 'Format............. hexadecimal' "$cli_output"
grep -Fq 'Private key........ not displayed or uploaded' "$cli_output"
grep -Fq 'Solution report.... pending review' "$cli_output"
grep -Fq '/api/puzzle/report-solution' "$curl_capture"
grep -Fq "\"assignment_id\":\"$assignment_id\"" "$curl_capture"
grep -Fq "\"client_id\":\"$client_id\"" "$curl_capture"
grep -Fq "$private_key" "$wallet_file"

if grep -Fq "$private_key" "$cli_output" || \
   grep -Fq "$private_key" "$notice_file" || \
   grep -Fq "$private_key" "$curl_capture"; then
  echo "Private key escaped the protected wallet file" >&2
  exit 1
fi

if grep -Eqi 'private[_ -]?key|wallet|result(s)?\.txt|found\.txt' "$curl_capture"; then
  echo "Solution report contains forbidden private-key metadata" >&2
  exit 1
fi

echo "KangarooSolutionRecoveryCliTests passed"

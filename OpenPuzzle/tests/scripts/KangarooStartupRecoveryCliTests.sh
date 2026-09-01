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

test_root="$(mktemp -d "${TMPDIR:-/tmp}/openpuzzle-kangaroo-cli.XXXXXX")"
test_home="$test_root/home"
bin_dir="$test_root/bin"
assignment_id="11111111-1111-4111-8111-111111111111"
previous_seed="0123456789ABCDEF"
workspace="$test_home/.local/share/OpenPuzzle/assignments/$assignment_id"
state_file="$test_home/.local/share/OpenPuzzle/client.state"
fake_engine="$test_root/psckangaroo"
engine_marker="$test_root/engine-started.txt"
curl_capture="$test_root/curl-arguments.txt"
cli_output="$test_root/openpuzzle-output.txt"
cli_pid=""
engine_pid=""
candidate_pid=""

cleanup() {
  if [[ -n "$cli_pid" ]] && kill -0 "$cli_pid" 2>/dev/null; then
    kill -KILL "$cli_pid" 2>/dev/null || true
    wait "$cli_pid" 2>/dev/null || true
  fi

  if [[ -n "$engine_pid" ]] && kill -0 "$engine_pid" 2>/dev/null; then
    kill -TERM -- "-$engine_pid" 2>/dev/null || \
      kill -TERM "$engine_pid" 2>/dev/null || true
    sleep 1
    kill -KILL -- "-$engine_pid" 2>/dev/null || \
      kill -KILL "$engine_pid" 2>/dev/null || true
  fi

  if [[ -n "$test_root" && -d "$test_root" ]]; then
    rm -rf -- "$test_root"
  fi
}
trap cleanup EXIT

install -d -m 700 "$workspace" "$bin_dir"

cat > "$fake_engine" <<'SH'
#!/bin/sh
printf 'pid=%s\n' "$$" > "$OPENPUZZLE_FAKE_ENGINE_MARKER"
printf 'arguments=%s\n' "$*" >> "$OPENPUZZLE_FAKE_ENGINE_MARKER"
exec /bin/sleep 30
SH
chmod 700 "$fake_engine"

cat > "$bin_dir/curl" <<'SH'
#!/bin/sh
printf '%s\n' "$*" >> "$OPENPUZZLE_FAKE_CURL_CAPTURE"
exit 0
SH
chmod 700 "$bin_dir/curl"

printf '%s\n' 'checkpoint-data' > "$workspace/kangaroo.checkpoint"
chmod 600 "$workspace/kangaroo.checkpoint"
printf '%s\n' \
  'HUNT: Speed: 2.75 GKeys/s | DPs: 42M | Time: 0d 01h 05m' \
  > "$workspace/kangaroo.log"
chmod 600 "$workspace/kangaroo.log"

cat > "$state_file" <<STATE
active=1
puzzle=140
range_id=9001
pid=999999
boot_id=
process_start_time=0
device=0
blocks=0
threads=0
points=0
profile_managed=0
assignment_id=$assignment_id
client_id=22222222-2222-4222-8222-222222222222
target=synthetic-address
public_key=031f6a332d3c5c4f2de2378c012f429cd109ba07d69690c6c701b6bb87860d6640
kangaroo_walk_seed=$previous_seed
kangaroo_generation=9
start=100000000
end=1FFFFFFFF
engine=Kangaroo
backend=CUDA
gpu_name=Synthetic GPU
workspace=$workspace
command=touch command-must-never-run
STATE
chmod 600 "$state_file"

read_field() {
  local name="$1"
  sed -n "s/^${name}=//p" "$state_file" | tail -n 1
}

env \
  HOME="$test_home" \
  PATH="$bin_dir:$PATH" \
  OPENPUZZLE_KANGAROO_PATH="$fake_engine" \
  OPENPUZZLE_FAKE_ENGINE_MARKER="$engine_marker" \
  OPENPUZZLE_FAKE_CURL_CAPTURE="$curl_capture" \
  stdbuf -oL -eL \
  "$openpuzzle_bin" run 140 --server https://server.test \
  > "$cli_output" 2>&1 &
cli_pid=$!

for _ in $(seq 1 100); do
  if [[ -f "$state_file" ]]; then
    candidate_pid="$(read_field pid)"
    if [[ "$candidate_pid" =~ ^[0-9]+$ ]] && \
       [[ "$candidate_pid" -gt 0 ]] && \
       [[ "$candidate_pid" -ne 999999 ]]; then
      engine_pid="$candidate_pid"

      if [[ -s "$engine_marker" ]]; then
        break
      fi
    fi
  fi

  if ! kill -0 "$cli_pid" 2>/dev/null; then
    echo "OpenPuzzle exited before recovery completed" >&2
    sed -n '1,240p' "$cli_output" >&2 || true
    exit 1
  fi

  sleep 0.1
done

if [[ -z "$engine_pid" || ! -s "$engine_marker" ]]; then
  echo "Kangaroo recovery did not produce both PID and engine marker" >&2
  printf 'Candidate PID....... %s\n' "${candidate_pid:-missing}" >&2
  printf 'Engine marker....... %s\n' "$([[ -s "$engine_marker" ]] && echo present || echo missing)" >&2
  echo "===== CLIENT STATE =====" >&2
  sed -n '1,240p' "$state_file" >&2 || true
  echo "===== CURL CAPTURE =====" >&2
  sed -n '1,240p' "$curl_capture" >&2 || true
  echo "===== OPENPUZZLE OUTPUT =====" >&2
  sed -n '1,240p' "$cli_output" >&2 || true
  echo "===== ENGINE MARKER =====" >&2
  sed -n '1,80p' "$engine_marker" >&2 || true
  echo "===== BITCRACK LOG =====" >&2
  sed -n '1,160p' "$workspace/bitcrack.log" >&2 || true
  echo "===== KANGAROO LOG =====" >&2
  sed -n '1,160p' "$workspace/kangaroo.log" >&2 || true
  echo "===== RELATED PROCESSES =====" >&2
  pgrep -af 'openpuzzle|psckangaroo|sleep 30' >&2 || true
  exit 1
fi

if ! kill -0 "$engine_pid" 2>/dev/null; then
  echo "Persisted Kangaroo supervisor PID is not running: $engine_pid" >&2
  echo "===== OPENPUZZLE OUTPUT =====" >&2
  sed -n '1,240p' "$cli_output" >&2 || true
  echo "===== ENGINE MARKER =====" >&2
  sed -n '1,80p' "$engine_marker" >&2 || true
  echo "===== BITCRACK LOG =====" >&2
  sed -n '1,160p' "$workspace/bitcrack.log" >&2 || true
  echo "===== KANGAROO LOG =====" >&2
  sed -n '1,160p' "$workspace/kangaroo.log" >&2 || true
  echo "===== RELATED PROCESSES =====" >&2
  pgrep -af 'openpuzzle|psckangaroo|sleep 30' >&2 || true
  exit 1
fi

[[ -s "$engine_marker" ]]
[[ -n "$(read_field boot_id)" ]]
[[ "$(read_field process_start_time)" =~ ^[1-9][0-9]*$ ]]
recovered_seed="$(read_field kangaroo_walk_seed)"
[[ "$recovered_seed" =~ ^[0-9A-F]{16}$ ]]
[[ "$recovered_seed" != "$previous_seed" ]]
[[ "$(read_field kangaroo_generation)" == "10" ]]

recovered_command="$(read_field command)"
[[ "$recovered_command" == *"-loadwild"* ]]
[[ "$recovered_command" == *"kangaroo.checkpoint"* ]]
[[ "$recovered_command" == *"-seed '$recovered_seed'"* ]]
[[ "$recovered_command" != *"command-must-never-run"* ]]
[[ -s "$workspace/kangaroo.checkpoint" ]]
[[ -s "$curl_capture" ]]
grep -Fq '/api/range/progress' "$curl_capture"
grep -Fq 'keys_checked' "$curl_capture"

kill -KILL "$cli_pid"
wait "$cli_pid" 2>/dev/null || true
cli_pid=""

kill -TERM -- "-$engine_pid" 2>/dev/null || \
  kill -TERM "$engine_pid" 2>/dev/null || true
sleep 1
if kill -0 "$engine_pid" 2>/dev/null; then
  kill -KILL -- "-$engine_pid" 2>/dev/null || \
    kill -KILL "$engine_pid" 2>/dev/null || true
fi
engine_pid=""

# Kangaroo double recovery regression: retain the same checkpoint while
# rotating the walk seed and incrementing the generation a second time.
checkpoint_hash_before="$(sha256sum "$workspace/kangaroo.checkpoint" | awk '{print $1}')"

sed -i \
  -e 's/^active=.*/active=1/' \
  -e 's/^pid=.*/pid=999999/' \
  -e 's/^boot_id=.*/boot_id=/' \
  -e 's/^process_start_time=.*/process_start_time=0/' \
  "$state_file"

rm -f -- \
  "$workspace/process.pid" \
  "$workspace/process.boot_id" \
  "$workspace/process.start_time" \
  "$workspace/exit.code"

: > "$cli_output"

env \
  HOME="$test_home" \
  PATH="$bin_dir:$PATH" \
  OPENPUZZLE_KANGAROO_PATH="$fake_engine" \
  OPENPUZZLE_FAKE_CURL_CAPTURE="$curl_capture" \
  "$openpuzzle_bin" run 140 --server https://server.test \
  > "$cli_output" 2>&1 &
cli_pid=$!

for _ in $(seq 1 100); do
  if [[ -f "$state_file" ]]; then
    candidate_pid="$(read_field pid)"
    if [[ "$candidate_pid" =~ ^[0-9]+$ ]] && \
       [[ "$candidate_pid" -gt 0 ]] && \
       [[ "$candidate_pid" -ne 999999 ]]; then
      engine_pid="$candidate_pid"
      break
    fi
  fi

  if ! kill -0 "$cli_pid" 2>/dev/null; then
    echo "OpenPuzzle exited before second recovery completed" >&2
    sed -n '1,240p' "$cli_output" >&2 || true
    exit 1
  fi

  sleep 0.1
done

[[ -n "$engine_pid" ]] || {
  echo "Second Kangaroo recovery did not persist a new PID" >&2
  sed -n '1,240p' "$cli_output" >&2 || true
  exit 1
}

kill -0 "$engine_pid"
second_seed="$(read_field kangaroo_walk_seed)"
[[ "$second_seed" =~ ^[0-9A-F]{16}$ ]]
[[ "$second_seed" != "$previous_seed" ]]
[[ "$second_seed" != "$recovered_seed" ]]
[[ "$(read_field kangaroo_generation)" == "11" ]]

second_command="$(read_field command)"
[[ "$second_command" == *"-loadwild"* ]]
[[ "$second_command" == *"kangaroo.checkpoint"* ]]
[[ "$second_command" == *"-seed '$second_seed'"* ]]
[[ "$second_command" != *"command-must-never-run"* ]]
[[ "$(sha256sum "$workspace/kangaroo.checkpoint" | awk '{print $1}')" == \
   "$checkpoint_hash_before" ]]

kill -KILL "$cli_pid"
wait "$cli_pid" 2>/dev/null || true
cli_pid=""

kill -TERM -- "-$engine_pid" 2>/dev/null || \
  kill -TERM "$engine_pid" 2>/dev/null || true
sleep 1
if kill -0 "$engine_pid" 2>/dev/null; then
  kill -KILL -- "-$engine_pid" 2>/dev/null || \
    kill -KILL "$engine_pid" 2>/dev/null || true
fi
engine_pid=""

echo "Kangaroo generations 9 -> 10 -> 11 passed"
echo "Kangaroo seeds 3/3 different passed"
echo "Kangaroo checkpoint preservation passed"

echo "KangarooStartupRecoveryCliTests passed"

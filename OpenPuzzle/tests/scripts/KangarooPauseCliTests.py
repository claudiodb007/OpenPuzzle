#!/usr/bin/env python3
"""Paused Kangaroo recovery must never silently claim replacement work."""
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile

def quoted(value):
    return '"' + value.replace('\\', '\\\\').replace('"', '\\"') + '"'

def main():
    cli = str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix='openpuzzle-paused-cli-') as temp:
        root = Path(temp)
        assignment = '11111111-1111-4111-8111-111111111111'
        client = '22222222-2222-4222-8222-222222222222'
        data = root / '.local/share/OpenPuzzle'
        workspace = data / 'assignments' / assignment
        workspace.mkdir(parents=True, mode=0o700)
        binary = root / 'bin'; binary.mkdir(mode=0o700)
        kangaroo = binary / 'psckangaroo'
        kangaroo.write_text('#!/bin/sh\nexit 99\n')
        kangaroo.chmod(0o700)
        capture = root / 'http.log'
        curl = binary / 'curl'
        curl.write_text('''#!/bin/sh
printf '%s\\n' "$*" >> "$PAUSE_TEST_HTTP"
case "$PAUSE_TEST_MODE" in
  temporary) printf '%s\n' 'temporary network failure'; exit 28 ;;
  rejected) printf '%s\\n' '{"error":"assignment_lease_expired","message":"expired"}'; exit 22 ;;
  *) exit 0 ;;
esac
''')
        curl.chmod(0o700)
        checkpoint = workspace / 'kangaroo.checkpoint'
        checkpoint.write_bytes(b'OPZKCP1\0' + bytes(192)); checkpoint.chmod(0o600)
        log = workspace / 'kangaroo.log'
        log.write_text('HUNT: Speed: 2.75 GKeys/s | DPs: 42M | Time: 0d 01h 05m\n'); log.chmod(0o600)
        exit_marker = workspace / 'exit.code'; exit_marker.write_text('0\n'); exit_marker.chmod(0o600)
        state = data / 'client.state'
        public_key = '031f6a332d3c5c4f2de2378c012f429cd109ba07d69690c6c701b6bb87860d6640'
        fields = dict(active=1, puzzle=140, range_id=9001, pid=999999, boot_id='',
                      process_start_time=0, device=0, blocks=0, threads=0, points=0, profile_managed=0,
                      assignment_id=assignment, client_id=client, target='synthetic-address', public_key=public_key,
                      kangaroo_walk_seed='0123456789ABCDEF', kangaroo_generation=9,
                      start='100000000', end='1FFFFFFFF', engine='Kangaroo', backend='CUDA',
                      gpu_name='Synthetic GPU', workspace=str(workspace), command='must-never-launch')
        state.write_text(''.join(f'{key}={value}\n' for key,value in fields.items())); state.chmod(0o600)
        binding = (f'{quoted(assignment)} {quoted(client)} 999999 "" 0 0 9 "0123456789ABCDEF" '
                   f'"100000000" "1FFFFFFFF" {quoted(public_key)}')
        intent = workspace / 'kangaroo.pause'
        intent.write_text(f'OP-KANGAROO-PAUSE-1\npaused\n{quoted(binding)}\n999999\n"old-owner"\n1\n')
        intent.chmod(0o600)
        env = {k:v for k,v in os.environ.items() if not k.startswith(('OPENPUZZLE_', 'CUDA_', 'OPV_'))}
        env.update(HOME=str(root), PATH=str(binary)+os.pathsep+env.get('PATH',''),
                   XDG_DATA_HOME=str(root/'.local/share'), XDG_CONFIG_HOME=str(root/'.config'),
                   OPENPUZZLE_KANGAROO_PATH=str(kangaroo), PAUSE_TEST_HTTP=str(capture), PAUSE_TEST_MODE='rejected')
        def run(*args):
            return subprocess.run([cli,*args], env=env, capture_output=True, text=True, timeout=12)
        def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
        before = {p:digest(p) for p in (state,checkpoint,intent,exit_marker)}
        status = run('status'); assert status.returncode==0 and 'paused' in status.stdout, status
        result = run('run','140','--engine','kangaroo','--backend','cuda','--resume-paused','--once')
        output = result.stdout + result.stderr
        assert result.returncode==1 and 'No new assignment requested' in output, output
        assert before == {p:digest(p) for p in before}, 'Refused lease changed paused data'
        calls = capture.read_text(); assert '/api/range/progress' in calls and '/api/range/claim' not in calls, calls
        env['PAUSE_TEST_MODE']='temporary'
        pending = run('stop')
        assert pending.returncode==1 and state.exists() and checkpoint.exists(), pending.stdout+pending.stderr
        calls = capture.read_text(); count = len(calls)
        blocked = run('run','140','--engine','kangaroo','--backend','cuda','--resume-paused','--once')
        assert blocked.returncode==1 and len(capture.read_text())==count, blocked.stdout+blocked.stderr
        env['PAUSE_TEST_MODE']='accepted'
        cancelled = run('stop')
        assert cancelled.returncode==0 and not state.exists(), cancelled.stdout+cancelled.stderr
        assert checkpoint.exists() and digest(checkpoint)==before[checkpoint]
        calls = capture.read_text(); assert '/api/range/complete' in calls and '/api/range/claim' not in calls, calls
        count = len(calls)
        missing = run('run','140','--engine','kangaroo','--backend','cuda','--resume-paused','--once')
        assert missing.returncode==1 and len(capture.read_text())==count, missing.stdout+missing.stderr
        print('KangarooPauseCliTests passed: expired lease, no replacement claim, preserved checkpoint, explicit Stop')

if __name__ == '__main__':
    main()

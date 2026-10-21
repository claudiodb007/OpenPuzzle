#!/usr/bin/env python3
"""Exercise the actual Linux CLI with local synthetic engines; no driver/network use."""
import fcntl
import os
from pathlib import Path
import shutil
import signal
import sqlite3
import subprocess
import sys
import tempfile
import time


def wait_for(predicate, process, timeout=8):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if predicate():
            return
        if process.poll() is not None:
            raise RuntimeError('Synthetic benchmark exited prematurely')
        time.sleep(0.02)
    raise RuntimeError('Synthetic benchmark did not become ready')


def main():
    with tempfile.TemporaryDirectory(prefix='openpuzzle-benchmark-cli-') as directory:
        root = Path(directory)
        home = root / 'isolated-user'
        data = home / '.local/share/OpenPuzzle'
        data.mkdir(parents=True)
        engines = root / 'libexec/OpenPuzzle'
        engines.mkdir(parents=True)
        cli = root / 'openpuzzle'
        shutil.copy2(sys.argv[1], cli)
        env = {k: v for k, v in os.environ.items() if not k.startswith('OPENPUZZLE_')}
        env.update(HOME=str(home), XDG_CONFIG_HOME=str(home / '.config'),
                   XDG_DATA_HOME=str(home / '.local/share'), OPENPUZZLE_TEST_ROOT=str(root))
        env['PATH'] = str(root) + os.pathsep + env.get('PATH', '')
        smi = root / 'nvidia-smi'
        smi.write_text('#!/bin/sh\nprintf "0, Synthetic GPU, TEST-UUID, 1024 MiB, fixture\\n"\n')
        smi.chmod(0o700)
        engine = engines / 'cuBitCrack'
        engine.write_text('''#!/bin/bash
case "$1" in
 --openpuzzle-engine-version) echo 'OpenPuzzle-BitCrack 1.0.0 CUDA protocol 2'; exit 0;;
 --list-devices) printf 'ID: 0\\nName: Synthetic GPU\\nMemory: 1024 MB\\nCompute units: 2\\n'; exit 0;;
esac
printf '%s\\n' "$@" > "$OPENPUZZLE_TEST_ROOT/engine-argv"
printf '%s\\n' "$$" > "$OPENPUZZLE_TEST_ROOT/engine-pid"
if test -f "$OPENPUZZLE_TEST_ROOT/fail"; then exit 1; fi
limit=20
if test -f "$OPENPUZZLE_TEST_ROOT/hold"; then limit=3; fi
for ((i=1;i<=limit;i++)); do
 printf 'GPU | 1000.00 MKey/s (1000 total) [00:00:01]\\n'
done
if test -f "$OPENPUZZLE_TEST_ROOT/hold"; then
 trap 'exit 0' TERM
 while :; do sleep .05; done
fi
''')
        engine.chmod(0o700)
        args = [str(cli), 'benchmark', '--backend', 'cuda', '--gpu', '0', '--auto', '--real', '--require-idle']

        def run(arguments=args):
            return subprocess.run(arguments, env=env, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, text=True, timeout=30)

        def profiles():
            with sqlite3.connect(data / 'openpuzzle.db') as db:
                return db.execute('SELECT * FROM gpu_profiles ORDER BY id').fetchall()

        result = run()
        if result.returncode:
            raise RuntimeError(result.stdout)
        assert 'Saved profile..... yes' in result.stdout
        assert 'Validating finalist' in result.stdout
        baseline = profiles()
        assert len(baseline) == 1 and baseline[0][7] == 970.0, baseline
        assert (root / 'engine-argv').read_text().find('1BgGZ9tcN4rm9KBzDn7KprQz87SZ26SAMH') >= 0

        (root / 'fail').touch()
        result = run()
        assert result.returncode != 0 and profiles() == baseline
        (root / 'fail').unlink()

        # The cooperative cancellation must stop the owned child and preserve DB rows.
        (root / 'hold').touch()
        (root / 'engine-pid').unlink()
        log = root / 'cancel-output.txt'
        with log.open('w') as output:
            child = subprocess.Popen(args, env=env, stdout=output, stderr=subprocess.STDOUT)
            try:
                wait_for(lambda: (root / 'engine-pid').exists(), child)
                engine_pid = int((root / 'engine-pid').read_text())
                # Unit-buffered candidate output is visible before the process exits.
                assert 'Candidate matrix.' in log.read_text()
                assert child.poll() is None
                wait_for(lambda: len((data / 'benchmark/stdout.log').read_bytes().splitlines()) >= 3, child)
                previous_stdout = (data / 'benchmark/stdout.log').read_bytes()
                second = run()
                assert second.returncode != 0 and 'Another benchmark' in second.stdout
                assert (data / 'benchmark/stdout.log').read_bytes() == previous_stdout
                child.send_signal(signal.SIGTERM)
                assert child.wait(timeout=5) == 130
                deadline = time.monotonic() + 2
                while True:
                    try:
                        os.kill(engine_pid, 0)
                    except ProcessLookupError:
                        break
                    stat = Path('/proc') / str(engine_pid) / 'stat'
                    try:
                        if stat.read_text().rsplit(')', 1)[1].split()[0] == 'Z':
                            break  # exited grandchild awaiting init's reap
                    except FileNotFoundError:
                        break
                    if time.monotonic() >= deadline:
                        raise RuntimeError('Owned synthetic engine was left running')
                    time.sleep(0.02)
            finally:
                if child.poll() is None:
                    child.terminate()
                    child.wait(timeout=5)
        assert 'Benchmark cancelled.' in log.read_text()
        assert profiles() == baseline
        (root / 'hold').unlink()

        # An externally held lock and a symlink are rejected without deleting output.
        lock = data / 'benchmark/benchmark.lock'
        with lock.open('r+') as held:
            fcntl.flock(held, fcntl.LOCK_EX | fcntl.LOCK_NB)
            assert run().returncode != 0
        lock.unlink()
        target = root / 'unrelated'
        target.write_text('preserve me')
        lock.symlink_to(target)
        assert run().returncode != 0 and target.read_text() == 'preserve me'
        lock.unlink()
        assert profiles() == baseline
        # A subsequent successful measurement remains possible after cancellation.
        assert run().returncode == 0
        assert not list(data.glob('client*.json'))
        print('LinuxBenchmarkCliTests passed: real CLI automatic selection, DB save, failure/cancel preservation, owned-child shutdown, workspace lock and retry; synthetic engines only.')


if __name__ == '__main__':
    main()

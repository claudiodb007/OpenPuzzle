# OpenPuzzle 1.0.21 — Kangaroo runtime validation

OpenPuzzle 1.0.21 begins runtime validation of the external PSCKangaroo
executor without changing assignment fairness or the Puzzle 71 linear path.

## Local Kangaroo self-test

The client accepts:

```text
openpuzzle selftest --backend kangaroo --device 0
```

The command uses a synthetic 32-bit secp256k1 vector, the configured external
PSCKangaroo executable and no server connection. It never requests an
assignment and does not use a funded Bitcoin Puzzle.

The self-test:

- requires every OpenPuzzle runtime slot to be idle;
- limits PSCKangaroo to 4 GiB of RAM and 180 seconds;
- creates its workspace and output with owner-only permissions;
- compares the result locally with the expected synthetic key;
- removes the complete workspace after success;
- preserves only a protected diagnostic log after failure;
- never prints or transmits the private test value.

The external executor remains opt-in and is not included in the Debian
package. Production Kangaroo assignments remain fail-closed until the full
runtime, checkpoint and interruption lifecycle is validated.

## Assignment checkpoint contract

Production Kangaroo commands use an assignment-local
`kangaroo.checkpoint` file with owner-only permissions. PSCKangaroo saves the
checkpoint every hour and may load it only from the same protected assignment
workspace. A symbolic link or any non-regular checkpoint is rejected before
the external executor starts. Checkpoint data remains local and is never part
of progress, completion or solution-report requests.

The local execution state also persists the assignment's compressed public key
so a future recovery path can rebuild the Kangaroo command from validated
fields. Recovery never executes the persisted diagnostic `command` field.

## Kangaroo progress contract

Runtime synchronization reads PSCKangaroo telemetry only from `kangaroo.log`.
Speed is reported normally, while `keys_checked=0` explicitly means that
linear coverage is not applicable to Pollard Kangaroo. Clean exit and
checkpoint messages are never treated as proof that a Kangaroo interval was
exhausted.

## Checkpoint recovery planning

Before any relaunch, OpenPuzzle rebuilds a Kangaroo command from validated
state fields. The persisted diagnostic command is never executed. Recovery
requires the exact assignment workspace, a private regular checkpoint, a
direct executable, no exit marker and no pending result material. Unsafe or
incomplete state fails closed without changing local files.

Recovery also requires the last Kangaroo speed sample to be accepted by the
server with `keys_checked=0`. A temporary network failure leaves the checkpoint
stopped for retry; an expired or rejected assignment is never relaunched.

After both gates succeed, the recovery coordinator starts only the rebuilt
command, captures a new Linux process identity and atomically replaces the
local execution state. If state persistence fails, the new process is stopped
and the recovery fails closed.

At client startup, an interrupted Kangaroo assignment enters this recovery
flow before generic completion synchronization. Temporary network failures
retain the stopped checkpoint for retry, rejected leases remove only the active
state, and unsafe local recovery data remains preserved for diagnosis.

The public `openpuzzle run` path is covered by an offline process-level test.
It starts the compiled client with synthetic interrupted state, a fake server
and a fake executor, then verifies the rebuilt checkpoint command and the new
persisted Linux process identity without contacting a network or GPU.

The background supervisor now shell-quotes its complete child script and all
launcher-owned paths. This preserves Kangaroo's nested checkpoint conditionals
and quoted arguments while preventing workspace names from changing shell
syntax.

The offline startup recovery regression now restarts the same interrupted
Kangaroo checkpoint twice. It requires generations 9, 10 and 11 to use three
different explicit walk seeds while preserving the checkpoint byte-for-byte.

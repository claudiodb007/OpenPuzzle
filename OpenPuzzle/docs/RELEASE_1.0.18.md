# OpenPuzzle 1.0.18

OpenPuzzle 1.0.18 hardens Linux process identity against PID reuse
within the same system boot.

## Full Linux process identity

Persistent execution identity is now bound to:

- Linux boot ID;
- process ID (PID);
- `/proc/<pid>/stat` field 22 (`starttime`).

A numeric PID can be reused by Linux. The boot ID added in OpenPuzzle
1.0.17 prevents stale processes from a previous boot being mistaken for
current executions, while the process start time added in 1.0.18 also
distinguishes two different processes that receive the same PID during
one boot.

## Persistent client state

`client.state` now stores:

```text
pid=<pid>
boot_id=<boot-id>
process_start_time=<starttime>
```

States written by OpenPuzzle 1.0.17 or earlier remain readable, but an
execution state without `process_start_time` is not accepted as proof
that a process is still active.

Legacy or incomplete identities fail closed and enter the normal
recovery path.

## Execution workspace identity

Background execution workspaces now persist:

```text
process.pid
process.boot_id
process.start_time
```

The execution monitor and stopper require all three identity components
to match.

## Safe process stopping

`ExecutionStopper` revalidates the original process identity throughout
the stop sequence.

If the original process disappears or its start time no longer matches,
the execution is treated as gone. A reused numeric PID is not treated as
the original execution.

Identity is rechecked before termination and escalation operations,
including before SIGKILL.

## Runtime control

Runtime marker files now contain three fields:

```text
<pid>
<boot_id>
<process_start_time>
```

OpenPuzzle 1.0.17 two-field runtime markers and older PID-only markers
fail closed and are treated as stale.

`requestStop()` uses Linux pidfds for signal delivery after validating
the persisted process start time. This removes the PID-reuse race
between identity verification and SIGTERM delivery without persisting a
pidfd across OpenPuzzle processes.

## Update safety

The updater now considers a client execution active only when its
persisted boot ID, PID and process start time match the live process.

Runtime activity checks use the same full process identity.

## Compatibility

No server API or Bitcoin puzzle protocol change is required.

The additional state and marker fields are local Linux runtime metadata.

Older state remains parseable but cannot be used as positive process
identity without the new start-time field.

## Validation

OpenPuzzle 1.0.18 passed all 89 automated tests, including coverage for:

- exact live process identity;
- boot mismatch;
- same-boot PID reuse;
- missing legacy process start time;
- launcher start-time persistence;
- execution monitor identity validation;
- stopper protection against reused PIDs;
- runtime marker migration;
- pidfd signal delivery;
- updater active-execution validation.

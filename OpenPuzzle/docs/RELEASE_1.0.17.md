# OpenPuzzle 1.0.17

OpenPuzzle 1.0.17 hardens process identity across reboot, power loss and
Linux PID reuse.

## Persistent process identity now includes the system boot

Linux process identifiers are reused over time. A PID stored before a reboot
can therefore later refer to an unrelated process.

OpenPuzzle now persists the Linux system `boot_id` together with execution
process state. A stored process is considered potentially active only when:

- the stored `boot_id` exists;
- the current system `boot_id` can be read;
- both boot identifiers match; and
- the stored PID still exists.

If any identity check fails, the persisted PID is not trusted.

## Safe handling of legacy state

OpenPuzzle 1.0.16 and earlier did not persist a boot identifier.

Legacy execution state and PID-only runtime markers are therefore handled
conservatively. OpenPuzzle does not treat a numerically live PID as proof that
the old execution still exists.

This prevents an unrelated process that happens to reuse the same PID from
being mistaken for OpenPuzzle work.

## Runtime-control markers are boot-bound

The persistent runtime markers used by CPU, GPU, CUDA and OpenCL slots now
store both the runtime PID and the current `boot_id`.

Runtime discovery, safe-stop requests and direct stop requests validate both
values before acting on the stored PID.

A stale marker from a previous boot can be removed safely without signalling
the unrelated process that may now own that PID.

## Engine supervisors are boot-bound

Background execution workspaces now persist a `process.boot_id` alongside
`process.pid`.

The background launcher reads the current system boot identity before starting
the supervisor. If boot identity cannot be determined, the execution is not
launched.

Execution monitoring and `ExecutionStopper` require the workspace boot
identity to match the current boot before treating the supervisor as active or
sending signals to its process or process group.

## Recovery, updates and heartbeat use the same identity rules

Persistent PID checks are protected consistently across:

- interrupted-execution recovery;
- explicit execution stopping;
- runtime-control state;
- background supervisor monitoring;
- guarded update detection;
- client heartbeat status reporting.

This prevents a reboot followed by PID reuse from causing OpenPuzzle to report
an unrelated process as an active assignment or to signal it accidentally.

Direct signals to subprocesses created with `fork()` by the current running
OpenPuzzle process remain unchanged because those PIDs are not recovered from
persistent state.

## Compatibility and fairness

Normal successful execution, failure reporting, interrupted-assignment
recovery and concurrent CUDA/OpenCL operation remain unchanged.

Assignment fairness is unchanged. Process identity has no effect on priority,
reputation, scheduling rights or access to work.

## Validation

OpenPuzzle 1.0.17 passed all 88 automated tests.

Additional process-identity coverage verifies:

- persisted execution state round-trips its `boot_id`;
- a live numeric PID with a mismatched boot identity is rejected;
- legacy execution state without `boot_id` fails closed;
- stale runtime PID markers cannot signal a reused process;
- runtime markers persist the current system boot identity;
- workspace supervisors persist and validate `process.boot_id`;
- the execution monitor does not treat a reused PID from another boot as
  running;
- the execution stopper refuses to signal a process whose workspace boot
  identity does not match;
- heartbeat reporting ignores stale or legacy PID-only execution state;
- update safety checks require matching boot identity before reporting a
  persisted execution as active.

The complete automated suite remains at 88 passing tests.

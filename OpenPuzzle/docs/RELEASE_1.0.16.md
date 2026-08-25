# OpenPuzzle 1.0.16

OpenPuzzle 1.0.16 adds resilient recovery for client executions interrupted by
a reboot, power loss or other abrupt system shutdown.

## Interrupted executions are no longer mistaken for completion

OpenPuzzle stores a local execution state while an assignment is running. The
background supervisor normally writes `exit.code` before it terminates.

If the stored supervisor process has disappeared and no `exit.code` exists,
OpenPuzzle now classifies the execution as an abrupt interruption instead of
waiting indefinitely for a file that can never appear.

Interrupted executions use the internal exit code `-3` and are reported to the
coordination server with status `cancelled`.

## Progress and assignment state are preserved safely

The most recent available engine progress is included when an interrupted
assignment is reported.

If the coordination server is temporarily unavailable, the local execution
state is retained and synchronization can be retried later. OpenPuzzle does
not discard the assignment merely because the machine restarted or the
network is unavailable.

The local state is removed only after the server accepts the final report, or
when the server explicitly rejects the assignment as no longer valid.

## Concurrent CUDA and OpenCL recovery

CUDA and OpenCL runtime slots remain independent during recovery.

A failed or interrupted CUDA slot cannot remove or overwrite the OpenCL slot,
and vice versa. After a complete system restart, both stale slots can be
recovered independently.

Once each cancellation is acknowledged by the coordination server, its local
state is removed and that slot is free to request new work.

## Compatibility and fairness

Normal successful executions continue to report `completed` with exit code
`0`. Normal engine failures continue to report `failed` with their non-zero
exit code.

Existing single-backend and concurrent CUDA/OpenCL workflows remain
compatible.

Assignment fairness is unchanged. Recovery status does not affect priority,
reputation or access to work.

## Validation

OpenPuzzle 1.0.16 passed all 88 automated tests.

Additional recovery coverage verifies:

- dead process with missing `exit.code` is classified as interrupted;
- interrupted assignments use `cancelled` with exit code `-3`;
- temporary server failure preserves local state;
- successful server acknowledgement removes the recovered slot;
- CUDA and OpenCL states remain isolated;
- two interrupted GPU slots can be recovered independently;
- the HTTP completion payload carries the expected cancellation status,
  exit code and progress information.

The BitCrack third-party license used for release validation was also verified
against the upstream `brichard19/BitCrack` MIT license.

# OpenPuzzle 1.0.6

OpenPuzzle 1.0.6 adds a graceful runtime drain command for operators who want
to stop continuous execution without cancelling the range currently being
searched.

## Safestop

```bash
openpuzzle safestop
```

The command records a local stop-after-assignment request. OpenPuzzle
continues the active engine, progress uploads and normal completion
synchronization. Once the assignment is finalized, the runtime exits before
requesting another range.

Concurrent GPU and CPU execution is supported: each active slot drains
independently and no slot claims replacement work after its request has been
observed. The command is idempotent and can be repeated safely.

`openpuzzle stop` is unchanged and remains the immediate shutdown and
assignment-cancellation command.

## Compatibility

Safestop is local runtime control. It does not change the public server
protocol, assignment format, engine interfaces or range-completion rules.
Existing CUDA, OpenCL and CPU execution modes remain compatible.

## Public repository boundary

The public release contains only the OpenPuzzle client. Private server
implementation and deployment files are not part of this release.

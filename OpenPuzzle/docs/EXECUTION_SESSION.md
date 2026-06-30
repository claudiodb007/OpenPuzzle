# ExecutionSession

`ExecutionSession` represents one attempt to execute a job.

A single `Job` may have multiple execution sessions.

Example:

```text
Job 42
  Execution 1 -> interrupted
  Execution 2 -> resumed
  Execution 3 -> finished
```

This separation is important for recovery, auditing and future dashboard replay.

## Current fields

- `executionId`
- `jobId`
- `puzzleId`
- `rangeId`
- `engine`
- `workspace`
- `command`
- `status`

## Status values

- `CREATED`
- `RUNNING`
- `FINISHED`
- `FAILED`
- `CANCELLED`

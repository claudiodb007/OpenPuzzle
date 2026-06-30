# EventBus

`EventBus` is the internal event system for OpenPuzzle.

It allows core components to emit events without knowing who will consume them.

Example:

```text
ExecutionManager
  -> EventBus
      -> Dashboard
      -> Database telemetry
      -> Logger
      -> CommunitySync
```

## Current events

- `ExecutionStarted`
- `Speed`
- `Progress`
- `FoundKey`
- `ExecutionFinished`
- `Error`

## Test

```bash
./OpenPuzzle event-test
```

Expected output includes:

```text
EVENT................ EXECUTION_STARTED
EVENT................ SPEED
EVENT................ EXECUTION_FINISHED
Events received...... 3
```

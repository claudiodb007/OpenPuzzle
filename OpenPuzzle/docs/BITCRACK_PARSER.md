# BitCrack Output Parser

The BitCrack Output Parser extracts structured events from raw BitCrack stdout.

Supported events in this build:

- `Speed`
- `StartingKey`
- `EndingKey`
- `CountingBy`
- `Found`
- `Error`
- `Finished`

## Test command

```bash
./OpenPuzzle parse-bitcrack-line --line "[Info] 1334.62 MKey/s"
```

Expected result:

```text
Type................. SPEED
MKey/s............... 1334.62
```

This parser will be used by the Execution Engine to update SQLite statistics in real time.

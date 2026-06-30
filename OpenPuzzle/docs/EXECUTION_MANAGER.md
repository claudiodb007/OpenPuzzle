# ExecutionManager

`ExecutionManager` is the first orchestration layer above `ProcessRunner`.

Current responsibilities:

- execute a command;
- read output line by line;
- send each line to `BitCrackOutputParser`;
- summarize parsed events.

It still does **not** update SQLite. That comes in the next step.

## Test

```bash
./OpenPuzzle execution-test
```

Expected:

```text
Started.............. yes
Exit code............ 0
Lines................ 3
Speed events......... 1
Last speed........... 1334.62 MKey/s
Error events......... 0
Found events......... 0
Finished events...... 1
```

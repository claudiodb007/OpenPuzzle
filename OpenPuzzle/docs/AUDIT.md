# OpenPuzzle audit

OpenPuzzle 1.0.4 introduces a local, detailed audit listing backed by SQLite.

## Show recent entries

```bash
openpuzzle audit
```

The default output contains the 50 newest entries.

## Filters

```bash
openpuzzle audit --limit 100
openpuzzle audit --puzzle 71
openpuzzle audit --event assignment_completed
openpuzzle audit --puzzle 71 --event assignment_completed --limit 25
```

`--limit` accepts values from 1 to 1000. Puzzle and event filters are exact.

## Stored fields

Each entry can identify:

- timestamp;
- event;
- puzzle;
- range;
- job;
- execution;
- descriptive message.

Identifiers that do not apply to an event are stored as `NULL` and displayed
as `-`.

The schema is created automatically in:

```text
~/.local/share/OpenPuzzle/openpuzzle.db
```

Running `openpuzzle audit` is read-only apart from the normal idempotent schema
initialization performed by OpenPuzzle.

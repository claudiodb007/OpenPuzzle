# OpenPuzzle 1.0.20 — Puzzle-aware engine routing and resilient solution alerts

OpenPuzzle 1.0.20 adds metadata-driven routing between linear keyspace
searches and Pollard Kangaroo workloads while preserving the existing Puzzle
71 execution path.

## Puzzle-aware execution

- Puzzle 71 remains a linear workload using bundled BitCrack CUDA/OpenCL or
  KeyHunt CPU execution.
- The packaged catalogue adds puzzles 140, 145, 150, 155 and 160 with their
  compressed public keys, HASH160 values, exact keyspaces and mandatory CUDA
  routing metadata.
- Unknown, incomplete or contradictory puzzle metadata fails closed before an
  assignment is requested or an engine is launched.
- Kangaroo claims require exact power-of-two ranges and validate the returned
  assignment shape before execution.

These routing rules do not change assignment fairness. Hardware performance is
used only for compatible execution and range sizing, never for priority.

## External PSCKangaroo integration

The client includes a CUDA-only PSCKangaroo adapter, command construction,
progress parsing and a protected result-file bridge into OpenPuzzle's standard
`found.txt` workflow.

PSCKangaroo is deliberately not bundled in the Debian package. Installation is
an explicit user-local action:

```text
openpuzzle engine install psckangaroo
```

Without a validated external executable, Kangaroo routing fails closed before
registration or assignment claim. A valid external installation makes the
planner ready without changing the bundled BitCrack and KeyHunt engines.

## Resilient solution reporting

A possible solution remains entirely local. OpenPuzzle preserves the protected
workspace and wallet-import file and submits only the assignment identifier and
anonymous client identifier for review. Private-key material, engine output and
filesystem paths are never transmitted.

If the metadata-only report cannot be delivered because of a temporary network
failure, the client retries automatically every 30 seconds. Local state remains
preserved across interruption or restart until reporting succeeds.

## Compatibility and validation

- Existing linear clients and Puzzle 71 assignments remain compatible.
- Rusticl OpenCL selection and persistent process-identity protections from
  earlier releases remain in force.
- The complete client suite passes 98 of 98 automated tests.
- The targeted transient solution-report retry test passes.
- The validated release binary SHA-256 is
  `799950bb41097b027b040616d3c3c879c2a9d93b2c124c2a13c6902369a752c4`.

The Debian package contains BitCrack CUDA/OpenCL and KeyHunt. PSCKangaroo
remains an external, opt-in executor.

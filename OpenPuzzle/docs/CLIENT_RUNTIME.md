# Continuous Client Runtime

The OpenPuzzle client can request, execute and synchronize assignments
continuously. Search engines perform the cryptographic work; OpenPuzzle
coordinates assignment lifecycle, progress, recovery and finalization.

## Commands

Start continuous execution using the lowest-numbered active unsolved puzzle:

```bash
openpuzzle run
```

On first use, `run` selects an available bundled GPU backend and creates a
validated local benchmark profile before any registration, heartbeat or
assignment request. Later runs reuse that profile. `--dry-run` never starts
the benchmark or contacts the server.

Request work for a specific puzzle:

```bash
openpuzzle run 71
```

Execute only one assignment:

```bash
openpuzzle run --once
```

Inspect local execution state:

```bash
openpuzzle status
```

Stop the active runtime and cancel its current assignment safely:

```bash
openpuzzle stop
```

Preview local configuration without registration, heartbeat, assignment or
lease creation:

```bash
openpuzzle run --dry-run
```

## Continuous lifecycle

The continuous runtime performs this cycle:

1. Validate local configuration and create a GPU profile when required.
2. Register the client and publish its capabilities.
3. Request a random non-overlapping assignment.
4. Start the configured search engine.
5. Upload public progress metrics and renew the assignment lease.
6. Upload completion, failure or cancellation.
7. Request another assignment.

When no work is available, the client remains in the `waiting` state and
retries without exiting.

Client performance may be used to size an assignment and select a compatible
engine or backend. It must never grant scheduling priority, reputation-based
access or preferential treatment.

## Local states

`openpuzzle status` can report:

- `idle`: no runtime and no local execution;
- `waiting`: the continuous runtime is waiting for work;
- `running`: an engine is processing an assignment;
- `stopped`: the engine stopped and synchronization is pending;
- `solution found`: a non-empty local `found.txt` was detected.

## Recovery

Assignment state is stored in:

```text
~/.local/share/OpenPuzzle/client.state
```

If OpenPuzzle terminates while the engine remains active, a later
`openpuzzle run` attaches to the same process and assignment.

If the engine already terminated, OpenPuzzle synchronizes its final state
before requesting new work. Temporary network failures retain local state and
are retried.

Assignments rejected permanently by the server are stopped and released
locally. Invalid local or protocol state is preserved for diagnosis.

## Cancellation

`Ctrl+C`, `SIGTERM` and `openpuzzle stop` request an orderly shutdown:

1. Synchronize final public progress when possible.
2. Stop the complete engine process group.
3. Report cancellation with the final key counter.
4. Preserve searched coverage on the server.
5. Remove local active state only after a safe final response.

If the server already rejected or finalized the assignment, openpuzzle stops
the engine without repeatedly submitting the same transition.

## Solution safety

Each assignment uses:

```text
~/.local/share/OpenPuzzle/assignments/<assignment-id>/
```

A non-empty `found.txt` is treated as a potential solution.

When detected, OpenPuzzle:

- stops continuous execution;
- stops the engine if it is still active;
- preserves `client.state`, `found.txt` and the entire workspace;
- validates the structured result locally and checks that its address matches
  the assignment;
- creates `~/OpenPuzzle-Solutions/Puzzle-N/<assignment-id>/wallet-import.txt`
  with a compressed WIF key;
- protects solution directories with mode `700` and the wallet file with mode
  `600`;
- displays only filesystem paths and never displays or uploads the private key;
- submits only `assignment_id` and the anonymous `client_id` for review;
- prevents `openpuzzle stop` from deleting the preserved solution.

The wallet-import file is designed for local import into a trusted wallet. The
operator should first make a secure offline backup and verify the address.
Never paste a private key into a website, chat, issue, log or untrusted
application.

The metadata-only report is stored as `pending`. It does not complete the
assignment, change range allocation, stop the scheduler or mark the puzzle as
solved. Repeated reports for the same assignment are idempotent.

The report endpoint accepts exactly two fields:

```json
{
  "assignment_id": "<assignment UUID>",
  "client_id": "<anonymous client UUID>"
}
```

Private keys, solution contents, filesystem paths and raw engine output are
rejected by the server.

An operator must independently verify a potential solution using trusted
offline tooling and public blockchain data. A report may then be marked
`verified` or `rejected`. Marking a puzzle as solved remains a distinct
administrative action and is never triggered automatically by a client report.


## Filesystem protection

OpenPuzzle protects sensitive local storage using owner-only permissions:

- configuration and data directories: `0700`;
- identity, configuration, state, PID and SQLite files: `0600`;
- assignment workspaces: `0700`;
- engine result, log and lifecycle files: created under `umask 077`.

These permissions are applied both when files are created and when existing
local state is loaded.

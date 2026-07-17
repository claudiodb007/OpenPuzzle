# Continuous Client Runtime

The OpenPuzzle client can request, execute and synchronize assignments
continuously. Search engines perform the cryptographic work; OpenPuzzle
coordinates assignment lifecycle, progress, recovery and finalization.

## Commands

Start continuous execution using the lowest-numbered active unsolved puzzle:

```bash
OpenPuzzle run
```

Request work for a specific puzzle:

```bash
OpenPuzzle run 71
```

Execute only one assignment:

```bash
OpenPuzzle run --once
```

Inspect local execution state:

```bash
OpenPuzzle status
```

Stop the active runtime and cancel its current assignment safely:

```bash
OpenPuzzle stop
```

Preview local configuration without registration, heartbeat, assignment or
lease creation:

```bash
OpenPuzzle run --dry-run
```

## Continuous lifecycle

The continuous runtime performs this cycle:

1. Register the client and publish its capabilities.
2. Request a random non-overlapping assignment.
3. Start the configured search engine.
4. Upload public progress metrics and renew the assignment lease.
5. Upload completion, failure or cancellation.
6. Request another assignment.

When no work is available, the client remains in the `waiting` state and
retries without exiting.

Client performance may be used to size an assignment and select a compatible
engine or backend. It must never grant scheduling priority, reputation-based
access or preferential treatment.

## Local states

`OpenPuzzle status` can report:

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
`OpenPuzzle run` attaches to the same process and assignment.

If the engine already terminated, OpenPuzzle synchronizes its final state
before requesting new work. Temporary network failures retain local state and
are retried.

Assignments rejected permanently by the server are stopped and released
locally. Invalid local or protocol state is preserved for diagnosis.

## Cancellation

`Ctrl+C`, `SIGTERM` and `OpenPuzzle stop` request an orderly shutdown:

1. Synchronize final public progress when possible.
2. Stop the complete engine process group.
3. Report cancellation with the final key counter.
4. Preserve searched coverage on the server.
5. Remove local active state only after a safe final response.

If the server already rejected or finalized the assignment, OpenPuzzle stops
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
- displays only the solution file path;
- does not read, print or upload the private key;
- prevents `OpenPuzzle stop` from deleting the preserved solution.

The operator should make a secure offline backup and verify the result with a
trusted offline tool. Never paste a private key into a website, chat, issue,
log or untrusted application.

Automatic server-side solution proof and verification are intentionally
separate from the normal progress and completion endpoints.

## Filesystem protection

OpenPuzzle protects sensitive local storage using owner-only permissions:

- configuration and data directories: `0700`;
- identity, configuration, state, PID and SQLite files: `0600`;
- assignment workspaces: `0700`;
- engine result, log and lifecycle files: created under `umask 077`.

These permissions are applied both when files are created and when existing
local state is loaded.

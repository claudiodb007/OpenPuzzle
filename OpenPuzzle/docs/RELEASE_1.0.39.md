# OpenPuzzle 1.0.39 — Preserve confirmed desktop state when status queries fail

OpenPuzzle 1.0.39 corrects the desktop interface's handling of failed status
queries. In 1.0.38, a command error could be interpreted as an idle client,
clearing visible runtime details and enabling new work. Controls could also be
enabled before the first status response. Buffered idle output from a crashed or
timed-out query could hide the failure.

## Status warnings and retained data

The interface now shows `Checking status` until its first valid response, and
`Status unavailable` when the query fails. It requires a normal command exit with
code zero, a nonempty response and recognized status for every reported slot.
The older supervisor-only format with a positive runtime PID remains supported.
Empty, malformed or incomplete responses do not confirm an idle client.

Command failures, process errors and the existing ten-second timeout block new
searches, benchmarks, self-tests, Kangaroo installation and thermal settings.
Partial output from a failed, crashed or timed-out status process is rejected.
The timeout terminates only the interface's status subprocess.

Last confirmed assignment details, slot cards, solution state and thermal
readings/history remain visible. Query diagnostics appear separately from those
retained details. Changing language preserves both the warning and its reason
in English, Portuguese, French and Spanish.

## Recovery and controls

Refresh and diagnostics remain available when the CLI can be started. Stop
controls retain their established availability for previously confirmed occupied
slots, including Kangaroo's safe-stop restriction. The interface does not infer
new occupied slots from a failed query or report that failure as a stopped engine.

A later valid response updates the retained data and restores the normal
controls, or the process-identity warning introduced in 1.0.38. Runtime control,
assignment handling, engines and server protocols retain their existing behavior.

## Validation

The correction passed all 141 native client tests on Ubuntu before this release
identity was applied. The release build reruns the full suite against the
versioned source before packaging. The UI executable compiled and all seven UI
suites also passed during local preparation.

The new UiStatusFailureTests covers pending startup, nonzero exit, empty and
malformed responses, unrecognized states, invalid PIDs, incomplete mixed slots,
crashes, process read errors, failure to start and timeout with buffered idle
output. It checks preservation of confirmed cards and thermal state, warnings
in all four languages, unavailable process identities and recovery to valid idle.
Disabled new-work buttons do not invoke the fixture CLI.

These interface regressions isolate configuration and local data and use a fake
CLI. They start no real search, request no assignment and change no installed
service. One new test suite raises the complete client count from 140 to 141.

## Upgrade

Allow active assignments to finish before replacing the installed client:

    openpuzzle safestop

Inspect any identity warning and wait for the relevant runtimes to become idle.
Then install the package:

    sudo apt install ./OpenPuzzle-1.0.39-Linux-x86_64.deb

Verify the installation and local configuration:

    openpuzzle --version
    openpuzzle doctor --offline
    openpuzzle status

Start searches explicitly using the established device and backend arguments.
The upgrade does not automatically start a search.

The portable updater package retains its established identity:

    OpenPuzzle-1.0.39-portable-XXXXXXXX.deb

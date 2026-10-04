# OpenPuzzle 1.0.34 — Recovery of stale execution identities

OpenPuzzle 1.0.34 fixes a recovery loop in which a live OpenPuzzle runtime
prevented synchronization of an execution whose persisted process identity
was already stale. A new continuous runtime could otherwise repeatedly report
that process identity was temporarily unavailable and never request new work.

## Process identity and recovery

The synchronizer distinguishes three outcomes:

- The persisted process is running with the same boot ID and start time.
  Normal public progress synchronization continues.
- The PID is absent, belongs to a different boot or has a different start time.
  The old execution enters recovery even while the current runtime owns its
  slot. Legacy incomplete identities retain the established recovery behavior.
- The identity of a potentially live process cannot be read. Local state is
  preserved and synchronization retries without uploading progress or
  completion. A failed boot-ID read is also treated as uncertain.

A process that disappears between the existence check and the identity read
is checked again. Only confirmed absence allows interruption synchronization;
an unreadable identity alone does not justify cancellation.

## Completion and local state

Without `exit.code`, a confirmed stale execution reports `cancelled`, exit code
`-3` and its last public progress. It never claims completed range coverage.
Successful completion still requires the established engine-specific proof
and exact assigned key count.

Accepted cancellation or explicit assignment rejection removes only the
affected slot's execution state. Temporary API failures and permanent local
or protocol errors preserve state for retry or diagnosis. Recovery leaves
other slots and the assignment workspace intact. Protected solution files
continue to take precedence and their contents are not sent by synchronization.

## Validation

The recovery correction passed all 137 client tests on Ubuntu before the
1.0.34 release identity was applied. The release build reruns the complete
suite against the versioned source before producing packages.

Expanded regression cases cover a live recovering runtime with an absent PID,
same-boot PID reuse, a different boot, accepted cancellation, explicit server
rejection, permanent protocol errors, temporary API failures, last-progress
preservation and isolation of another live slot. An injected identity reader
also verifies that genuine unreadability retains state with or without the
main runtime, even when an exit-code file is present.

The server protocol, private telemetry boundary, GPU selection, thermal
policy, desktop interface and bundled engines remain compatible.

## Upgrade

Allow active assignments to finish before replacing the installed client:

    openpuzzle safestop

When the relevant runtimes are idle, install the package:

    sudo apt install ./OpenPuzzle-1.0.34-Linux-x86_64.deb

Verify the installation and local configuration:

    openpuzzle --version
    openpuzzle doctor --offline
    openpuzzle status

Start searches explicitly using the established device and backend arguments.
The upgrade does not automatically start a search.

The portable updater package retains its established identity:

    OpenPuzzle-1.0.34-portable-XXXXXXXX.deb

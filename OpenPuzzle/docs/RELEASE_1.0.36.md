# OpenPuzzle 1.0.36 — Preserve occupied slots during unreadable runtime identity

OpenPuzzle 1.0.36 fixes a startup failure in which a temporarily unreadable
runtime identity could be mistaken for a free execution slot. Another
continuous runtime could otherwise replace the control marker of a process
that was still alive and discard its existing safe-stop request.

## Runtime acquisition

A complete runtime marker protects its slot when the current boot ID or the
saved process's start time cannot be read. A failed process-existence probe
also preserves control unless it positively establishes that the PID is absent.
The new runtime returns an error before clearing the safe-stop request or
replacing the existing marker; assignment state remains available to its owner.

The same decision is repeated after an exclusive-create collision. If another
runtime has appeared since the first inspection, a verified live identity or
an unreadable identity blocks takeover of that slot. Other execution slots
retain their own markers, assignments and safe-stop requests.

A confirmed process exit, a different boot or a different process start time
still permits normal stale-marker cleanup and acquisition. Incomplete legacy
markers retain their established cleanup behavior. The client rechecks process
existence if it exits during a failed identity read.

Read-only `running()` continues to mean a positively verified live identity.
An unreadable identity is not presented as running, but is preserved as a
reason to refuse acquisition. Failed-stop preservation from 1.0.35 remains
in place.

## Validation

The correction passed all 139 native client tests on Ubuntu before this
release identity was applied. The release build reruns the full suite against
the versioned source before packaging.

The added RuntimeAcquireFailureTests executable keeps a real child process
alive through a pipe and injects boot reads, process-identity reads, existence
probe results and an exclusive-create collision within that test target only.
It verifies repeated blocked acquisition, unchanged marker/request/assignment
contents, independent slots, no continuous-runtime callbacks after refusal,
confirmed stale cleanup and a process exiting during an identity read.

The test sends no termination signal, launches no engine and requires no GPU
or assignment server. Production process reads and public interfaces retain
their established behavior. Existing runtime and process-identity tests,
including the native pidfd stop path, remain part of the suite.

The recovery protocol, GPU selection, bundled engines and thermal settings
remain compatible with 1.0.35.

## Upgrade

Allow active assignments to finish before replacing the installed client:

    openpuzzle safestop

When the relevant runtimes are idle, install the package:

    sudo apt install ./OpenPuzzle-1.0.36-Linux-x86_64.deb

Verify the installation and local configuration:

    openpuzzle --version
    openpuzzle doctor --offline
    openpuzzle status

Start searches explicitly using the established device and backend arguments.
The upgrade does not automatically start a search.

The portable updater package retains its established identity:

    OpenPuzzle-1.0.36-portable-XXXXXXXX.deb

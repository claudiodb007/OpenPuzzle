# OpenPuzzle 1.0.35 — Runtime control survives failed stop requests

OpenPuzzle 1.0.35 fixes a stop-control failure that could erase the PID marker
of a runtime that was still alive. A denied or unavailable pidfd signal could
otherwise leave an active supervisor without its local control marker and
allow another runtime to acquire the same slot.

## Runtime identity and stop requests

A complete runtime marker is preserved when the current boot or process
identity cannot be read, or when sending the verified stop signal fails.
An absent PID, a different boot or a different process start time remains
positive evidence of a stale marker. Incomplete legacy markers retain their
established cleanup behavior and never authorize signalling by numeric PID.

A readable live identity continues to block a second runtime in the same
slot. Repeated failed stop requests preserve both the runtime marker and any
existing safe-stop request. Signalling still uses a pidfd and validates the
process instance; there is no numeric-PID signal fallback in runtime control.

## Command result and assignment state

When a failed stop leaves a runtime control marker in place, `openpuzzle stop`
reports the affected slot and returns a non-zero exit code. It preserves the
local assignment instead of attempting the legacy engine-stop fallback behind
the live supervisor.

Other GPU slots still receive their independent stop requests. If some requests
succeed and another fails, the command reports the successful requests and the
failure, rather than claiming that every runtime is shutting down.

## Validation

The correction passed all 138 native client tests on Ubuntu before this
release identity was applied. The release build reruns the full suite against
the versioned source before packaging.

The added RuntimeStopFailureTests executable injects process-identity reads
and signal outcomes only within that test target. It covers unavailable
syscalls, denied signals, a failed send reporting ESRCH while the saved process
is still alive, repeated stop attempts, duplicate acquisition, safe-stop
preservation, independent slots, unreadable identity, assignment preservation
and partial command failure. It sends no real signals and requires no GPU or
assignment server. Existing process-identity and runtime tests continue to
cover the real pidfd stop path.

The recovery protocol, GPU selection, bundled engines and thermal settings
remain compatible with 1.0.34.

## Upgrade

Allow active assignments to finish before replacing the installed client:

    openpuzzle safestop

When the relevant runtimes are idle, install the package:

    sudo apt install ./OpenPuzzle-1.0.35-Linux-x86_64.deb

Verify the installation and local configuration:

    openpuzzle --version
    openpuzzle doctor --offline
    openpuzzle status

Start searches explicitly using the established device and backend arguments.
The upgrade does not automatically start a search.

The portable updater package retains its established identity:

    OpenPuzzle-1.0.35-portable-XXXXXXXX.deb

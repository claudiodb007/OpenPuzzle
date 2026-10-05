# OpenPuzzle 1.0.40 — Keep desktop commands reserved across status updates

OpenPuzzle 1.0.40 corrects a desktop command overlap. In 1.0.39, a background
status response could re-enable Start and command buttons while a foreground
command was still running. For example, an idle status reply during a diagnostic
command could make new work available before that command had finished.

## Command reservation

The interface now tracks its foreground command separately from a pending
detached runtime launch. A foreground command keeps its reservation until its
own process finishes or fails to start. Background status replies, failed queries
and query timeouts do not release the command reservation.

New searches, other foreground commands, execution choices and thermal
configuration remain disabled while the command is in progress. Disabled buttons
do not invoke the CLI, and the command launcher also refuses overlapping requests.
Language changes preserve the disabled controls in all four interface languages.

Status polling continues to update the visible runtime state during a command.
A query timeout terminates only the status subprocess, leaving the foreground
command running. The existing status-unavailable warning and retained confirmed
data from 1.0.39 continue to apply.

## Completion and recovery

Normal completion, nonzero exit, process crash and failure to start release only
the matching foreground command's reservation. Failure to start is handled even
though the process does not emit a finished signal.

After release, button availability follows the confirmed runtime and identity
state. For example, an active runtime keeps Start disabled and restores its stop
controls; a confirmed idle client permits new work. The pending detached launch
retains its established status-based release and remains separate from foreground
command completion.

## Validation

The correction passed all 142 native client tests on Ubuntu before this release
identity was applied. The release build reruns the complete suite against the
versioned source before packaging. The UI executable compiled and all eight UI
suites also passed during local preparation.

The new UiCommandBusyTests holds a fake diagnostic command while delivering idle
and running status replies, a query error, a status timeout and a status process
that cannot start. It checks disabled command and configuration controls, blocked
button clicks, language changes and successful status recovery. Normal, failed,
crashed and unstartable foreground commands are checked for reservation release.

The fixture isolates configuration and local data and uses a fake CLI. It starts
no real search, requests no assignment and changes no installed service. The new
suite raises the complete client count from 141 to 142.

## Upgrade

Allow active assignments to finish before replacing the installed client:

    openpuzzle safestop

Inspect any identity warning and wait for the relevant runtimes to become idle.
Then install the package:

    sudo apt install ./OpenPuzzle-1.0.40-Linux-x86_64.deb

Verify the installation and local configuration:

    openpuzzle --version
    openpuzzle doctor --offline
    openpuzzle status

Start searches explicitly using the established device and backend arguments.
The upgrade does not automatically start a search.

The portable updater package retains its established identity:

    OpenPuzzle-1.0.40-portable-XXXXXXXX.deb

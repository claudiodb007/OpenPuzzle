# OpenPuzzle 1.0.41 — Keep pending desktop launches across older status replies

OpenPuzzle 1.0.41 corrects a desktop launch ordering problem. In 1.0.40, a
periodic status query could start while the client was idle, then finish after
the user clicked Start. That older idle response could release the launch
reservation and re-enable Start before any new query had observed the launch.

## Status query ordering

Each status query now records the launch generation at which it starts. A
successful detached launch advances that generation and reserves the interface.
A response from a query started before the launch cannot settle the new launch.

Older valid responses, process errors, crashes, empty or invalid output, read
errors and timeouts preserve the launch reservation and last confirmed status.
They do not replace runtime cards, solution state or thermal readings. Command
buttons, execution choices and thermal configuration remain disabled, including
across changes between the four interface languages. Disabled buttons do not
invoke another command or search.

A timeout still terminates only the status subprocess. After an older query
finishes, the interface schedules a fresh query; normal polling also continues.
The foreground command reservation introduced in 1.0.40 remains independent.

## Fresh status and recovery

A query started after the launch retains the established handling. A valid
running, idle or unavailable-identity response settles the pending reservation
and updates controls according to that observed state. A fresh query failure
reports unavailable status and blocks new searches until a valid query recovers.

The correction prevents a pre-launch observation from being treated as an
observation of the new launch. It does not prove runtime registration or process
liveness, and it does not change backend selection or assignment allocation.

## Validation

All 143 native client tests passed on Ubuntu before this release identity was
applied. The release build reruns the full suite against the versioned source
before packaging. The UI executable compiled and all nine UI suites passed
during local preparation.

The new UiLaunchStatusTests holds a fake status query before a detached launch,
then delivers an older idle response, nonzero exit, crash, empty or invalid
output, read error and timeout. It checks retained status, disabled controls,
blocked clicks, language changes and automatic refresh. Fresh running, idle and
unknown-identity responses, a fresh query failure and later recovery are checked.

The fixture isolates configuration and local data and uses a bounded fake CLI.
It runs no real search, requests no assignment and changes no installed service.
The new suite raises the complete client count from 142 to 143.

## Upgrade

Allow active assignments to finish before replacing the installed client:

    openpuzzle safestop

Inspect identity warnings and wait for the relevant runtimes to become idle.
Then install the package:

    sudo apt install ./OpenPuzzle-1.0.41-Linux-x86_64.deb

Verify the installation and local configuration:

    openpuzzle --version
    openpuzzle doctor --offline
    openpuzzle status

Start searches explicitly using the established device and backend arguments.
The upgrade does not automatically start a search.

The portable updater package retains its established identity:

    OpenPuzzle-1.0.41-portable-XXXXXXXX.deb

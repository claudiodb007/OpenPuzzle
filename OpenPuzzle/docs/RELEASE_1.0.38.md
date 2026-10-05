# OpenPuzzle 1.0.38 — Show unavailable execution identity in the desktop interface

OpenPuzzle 1.0.38 corrects the desktop interface's handling of the identity
warnings introduced in 1.0.37. An uncertain execution could appear stopped and
leave new-work controls enabled, or appear running solely because its status
included a runtime PID.

## Identity warnings

The interface now shows `Identity unavailable` when `status` reports an uncertain
engine or supervisor. Affected slot cards remain visible with an identity warning.
The global badge retains the warning when another worker is confirmed active.
An explicit warning is not overridden by the presence of a runtime PID.

This covers primary, legacy and dynamic CUDA/OpenCL slots, engine state without a
supervisor marker, and a stopped or running engine whose supervisor reports
`Runtime identity... unavailable`.

## Controls and recovery

Starting searches, benchmarking, self-tests, Kangaroo installation and thermal
configuration are disabled while any execution identity is unavailable. Refresh,
diagnostics, update checks and auditing remain available.

Stop controls can retry the existing CLI's identity-checked requests. Kangaroo
retains its safe-stop restriction. A later status that confirms running or idle
restores the established badge, slot cards and control availability. Changing
language preserves the warning and disabled controls in all four UI languages.

## Validation

The correction passed all 140 native client tests on Ubuntu before this release
identity was applied. The release build reruns the complete suite against the
versioned source before packaging.

UiControlTests now covers uncertain primary supervisors with and without a PID,
state-only engines, dynamic slots, uncertain supervisors beside stopped or active
engines, mixed worker states, four-language warnings, Kangaroo's stop restriction
and transitions back to confirmed running and idle states. Disabled new-work
buttons do not invoke the test CLI.

These interface regressions isolate configuration and local data and use a fake
CLI and fake systemctl. They start no search, request no assignment and change no
installed service. The test count remains 140 because the new cases extend an
existing suite. The six UI suites also passed during local preparation.

## Upgrade

Allow active assignments to finish before replacing the installed client:

    openpuzzle safestop

Inspect any identity warning and wait for the relevant runtimes to become idle.
Then install the package:

    sudo apt install ./OpenPuzzle-1.0.38-Linux-x86_64.deb

Verify the installation and local configuration:

    openpuzzle --version
    openpuzzle doctor --offline
    openpuzzle status

Start searches explicitly using the established device and backend arguments.
The upgrade does not automatically start a search.

The portable updater package retains its established identity:

    OpenPuzzle-1.0.38-portable-XXXXXXXX.deb

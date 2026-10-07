# OpenPuzzle 1.0.46 — Validate integer options before changing client controls

OpenPuzzle 1.0.46 groups CLI integer parsing and control-order corrections in one
commit. An invalid numeric argument now fails before the affected command changes
configuration, initializes its device or database context, acquires runtime control
or attempts assignment recovery. All 154 native client tests passed before this
release identity was prepared.

## Complete decimal values and shared aliases

The shared CliInteger parser requires the complete argument to be a decimal integer
within the supported int range. Missing or empty values, fractions, exponent syntax,
whitespace, trailing text and overflow are rejected. An absent option retains its
established default. Optional signs and leading zeroes remain accepted by the parser;
each command then applies its own bounds. GPU indices must be non-negative, while
puzzle identifiers and launch dimensions must be positive where required.

Aliases such as --device/-d, --gpu/-d and launch parameter long/short forms share
one parser in their respective commands. Supplying either form more than once is
rejected rather than choosing one conflicting value silently. These are numeric
option checks, not a replacement for every command's complete argument grammar.

## Validate before changing controls

Application GPU selection validates its integer argument before changing saved
settings. Benchmark validates numeric syntax before configuration loading, device
initialization or Rusticl updates. Numeric syntax errors return OP-BENCH-011; the
existing checks for backend, duration, samples and launch parameters remain in place.

StartJob checks puzzle, job, device and launch integers before creating its database
context. RunSession validates puzzle, GPU and OpenCL device selectors, launch settings,
CPU thread counts and assignment duration before starting multi-GPU or concurrent
supervisors, acquiring runtime control or recovering a preserved execution. The same
checks guard the individual run path. Invalid numeric requests cannot trigger those
operations merely because they use dry-run or a supervisor mode.

The accepted duration range remains 1 to 360 minutes, subject to the existing explicit
maximum-duration override. Benchmark bounds remain 10 to 300 seconds and 4 to 60 samples.
This release does not change search engines, saved benchmark profiles or server ranges.

## Validation

The grouped corrections were committed as
`6af7ebd7787505a717c0cef2b181b41ee80c9184` on published 1.0.45.
All 154 native client tests passed on Ubuntu, including desktop controls. JUnit names
were checked against the registered CTest tests, with no failures or skipped tests.
The release builder reruns the complete suite on the versioned source before packaging.

CliIntegerTests covers integer boundaries, complete decimal grammar, missing values,
defaults and duplicate aliases. CliArgumentIntegrityTests uses temporary configuration
and preserved-state fixtures to verify rejection before control changes, registration,
recovery, assignment requests or real searches. Existing valid GPU selection is also
covered. The two suites increase the registered count from 152 to 154.

The packages in this release are for Linux. A Windows build still requires work on
process launch and control, process identity, local file handling, GPU discovery and
packaging. This release does not provide a validated Windows executable.

## Upgrade

Allow assignments to finish before replacing the installed client:

    openpuzzle safestop

Wait for the relevant runtimes to become idle, then install and verify:

    sudo apt install ./OpenPuzzle-1.0.46-Linux-x86_64.deb
    openpuzzle --version
    openpuzzle doctor --offline
    openpuzzle status

Start searches explicitly with the established backend and device arguments.
The upgrade does not start a search automatically. The portable updater retains
its established package identity:

    OpenPuzzle-1.0.46-portable-XXXXXXXX.deb

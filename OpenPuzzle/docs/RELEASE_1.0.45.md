# OpenPuzzle 1.0.45 — Preserve configuration during control updates

OpenPuzzle 1.0.45 groups configuration and GPU-control corrections in one commit.
Updates preserve existing damaged settings, choosing a GPU retains supported
configuration fields, and invalid benchmark parameters cannot change Rusticl.
All 152 native client tests passed before preparing this release identity.

## Preserve existing settings when reading fails

Thermal CLI updates, desktop thermal saves, GPU selection and benchmark selector
updates previously could replace damaged settings with defaults. Checked loading
now distinguishes a missing config.json from an existing empty, malformed or
unreadable file. Only a missing file supplies creation defaults. Failed reads
preserve the existing bytes and report failure instead of silently repairing them.
Benchmark stops before context/device initialization with OP-BENCH-010.

Keep the original file when investigating a failure and recover known settings
from a backup before retrying. This release does not reconstruct settings already
lost before the upgrade. The general read-only load API retains its existing
fallback defaults; checked loading guards the update operations described above.

## Require an object and read without following the final symlink

Configuration documents must have a JSON object root. Arrays and scalar roots
are rejected instead of reading unrelated entries as settings. Existing flat and
nested object layouts and established typed-field defaults remain supported.

Reading uses a nonblocking descriptor, rejects non-regular files and final-path
symlinks, and changes permissions through the opened regular-file descriptor.
FIFOs cannot hang the configuration reader, and symlink targets are neither read
nor chmodded. This does not reject symlinks in parent directories or serialize
read/modify/write cycles against other processes. An explicit full save remains
available to callers that intentionally supply a complete configuration.

## Change the GPU without discarding supported settings

GPU selection previously replaced configuration with a minimal device document.
It now updates the device through ConfigurationManager and its atomic writer,
preserving supported engine/backend/executable paths, CUDA/OpenCL paths, thermal
policy, Rusticl selector and assignment duration. Negative device indices fail.
The CLI returns failure instead of printing a successful selection when loading
or saving fails. Saved-device reads share strict configuration parsing, so a
fractional number cannot select its numeric prefix as a GPU identifier.

Unknown configuration fields retain the existing full-save behavior and are not
preserved by this change. Atomic publication concerns one file and does not add
a transaction across configuration and assignment files or a power-loss guarantee
for directory entries.

## Validate benchmark arguments before changing Rusticl

Benchmark checks its backend, device, duration, sample count and launch parameters
before applying or persisting a Rusticl selector. Rejected arguments leave both
configuration bytes and the process Rusticl environment unchanged. The regression
tests use invalid requests before GPU discovery and do not run real benchmarks.

## Validation

The reproduced defects were committed together as
`be456eb9aa5d33cb6a707ab568c788257f8e350d`. All 152 native client tests passed on
Ubuntu, including desktop controls, before release preparation. The release build
reruns the full suite against versioned source before creating packages.

ConfigurationReadFailureTests and ConfigurationUpdateFailureTests raise the test
count from 150 to 152. They cover missing and damaged files, object-root validation,
typed device parsing, descriptor permissions, symlinks, directories, bounded FIFO
reads, supported settings across GPU changes, CLI failures and unchanged Rusticl
on invalid requests. UiThermalSettingsTests also covers preserved damaged files.
Tests use temporary homes and isolated children without registration, assignments
or real search engines.

## Upgrade

Allow active assignments to finish before replacing the installed client:

    openpuzzle safestop

Inspect runtime warnings and wait for the relevant runtimes to become idle.
Then install and verify:

    sudo apt install ./OpenPuzzle-1.0.45-Linux-x86_64.deb
    openpuzzle --version
    openpuzzle doctor --offline
    openpuzzle status

Start searches explicitly using the established backend and device arguments.
The upgrade does not automatically start a search. The portable updater retains
its established package identity:

    OpenPuzzle-1.0.45-portable-XXXXXXXX.deb

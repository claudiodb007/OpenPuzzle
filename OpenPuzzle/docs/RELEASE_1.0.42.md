# OpenPuzzle 1.0.42 — Preserve desktop controls and persisted client data

OpenPuzzle 1.0.42 groups five reproduced client defects in one correction commit.
It prevents stale desktop confirmations from invoking commands after the client
state changes and preserves data in configuration, execution recovery and thermal
history. All 146 native client tests passed before preparing this release identity.

## Kangaroo installation confirmation

Status polling continues while the installation confirmation is open. In 1.0.41,
a newly running client could disable the installation button, yet clicking Yes
in the already open dialog still invoked installation. The dialog now rechecks
current control availability before starting the command. Running slots, uncertain
identities and unavailable status block installation. Cancellation performs no
command, and installation remains available when current status allows it.

## Stop confirmation

A Stop confirmation could remain open after status changed from running to idle.
Clicking Yes then sent a stale Stop command. The dialog now rechecks Stop availability
after confirmation. Newly idle clients receive no command. Existing Stop retries
for occupied slots with unavailable identity or status remain supported, together
with the established Kangaroo restrictions and foreground command reservation.

## Thermal history preservation

A syntactically valid history file could contain malformed entries. Previously,
loading skipped those entries and a later append could overwrite the file without
them. Any malformed entry now puts the entire file into recovery state, including
an invalid entry following valid records. The original bytes and existing in-memory
history remain intact, and appending is blocked until the user explicitly clears
history. Missing optional fields retain legacy defaults; invalid field types are
rejected. Valid history retains its existing bounded storage and deduplication.

## Configuration string decoding

Configuration paths containing quotes or backslashes were saved with escapes but
were incorrectly read back. String fields are now decoded from their JSON values,
preserving quoted paths, backslashes, control characters and escaped Unicode.
Existing nested and flat legacy configuration layouts remain supported. Saved
string values consistently escape JSON control characters.

## Execution metadata and recovery

Execution metadata previously inserted engine, command and workspace strings into
JSON without escaping them. Quotes or control characters could make the file invalid,
and backslashes could change recovered command contents. Metadata now escapes JSON
strings, and recovery decodes them without losing their original contents. This
preserves newly stored data; it does not reconstruct data already lost in an older
file. Engine routing, assignment allocation and recovered-command execution rules
retain their established behavior.

## Validation

The five defects were reproduced against published 1.0.41 before applying their
corrections. They were committed together as
`fa8a6a6d90bb6a808c5159c0de154e2286f3f9f6`. All 146 native client tests passed on
Ubuntu, including the desktop UI, before this release identity was prepared.
The release build reruns the complete suite against versioned source before packaging.

Three new suites, UiConfirmationTests, ConfigurationJsonRoundTripTests and
ExecutionPersistenceRoundTripTests, raise the count from 143 to 146. Existing
UiThermalHistoryTests cover malformed entries and byte preservation. The confirmation
fixture checks status changes, query failures, timeouts, cancellation and language
changes with a bounded fake CLI. Persistence tests write and read temporary files;
they do not execute the stored commands. The new fixtures request no assignments
and start no real search. All 17 relevant suites passed during local preparation.

## Upgrade

Allow active assignments to finish before replacing the installed client:

    openpuzzle safestop

Inspect identity warnings and wait for the relevant runtimes to become idle.
Then install the package:

    sudo apt install ./OpenPuzzle-1.0.42-Linux-x86_64.deb

Verify the installation and local configuration:

    openpuzzle --version
    openpuzzle doctor --offline
    openpuzzle status

Start searches explicitly using the established device and backend arguments.
The upgrade does not automatically start a search.

The portable updater package retains its established identity:

    OpenPuzzle-1.0.42-portable-XXXXXXXX.deb

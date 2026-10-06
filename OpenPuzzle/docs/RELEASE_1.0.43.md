# OpenPuzzle 1.0.43 — Preserve local files and numeric recovery

OpenPuzzle 1.0.43 groups six reproduced persistence defects in one correction commit.
It prevents failed writes from replacing complete local files, preserves saved
command contents and makes configuration and recovery follow complete JSON fields.
All 149 native client tests passed before preparing this release identity.

## Complete configuration documents

An incomplete JSON configuration previously could supply numeric or boolean settings
through textual prefix searches even when its string fields could not be parsed.
Loading now parses the complete document once before applying any setting. Invalid
JSON returns the established defaults without rewriting the original file. Numeric
values with trailing text, fractional device IDs and non-finite thresholds retain
their defaults. Existing flat and nested configuration layouts remain supported.
This does not make subsequent explicit configuration changes a repair for lost data.

## Numbers independent of regional formatting

JSON writers previously inherited the global stream locale, which could insert
comma decimal separators or grouped integers and produce invalid JSON. Configuration,
execution metadata and client-state writers now use locale-independent numbers.
Finite doubles use their shortest round-trip decimal representation, retaining both
precision and simple values such as 1334.62. Non-finite configuration values are
rejected; a non-finite execution speed does not replace an existing state file.

## Preserve files when a write fails

Configuration and execution metadata previously opened destination files directly,
so a short or interrupted write could truncate the previous complete file. Writers
now prepare a complete owner-only temporary file in the same directory, check writes,
flush and close, then publish it by rename. Client-state saves use the same method
and no longer remove the destination after a failed rename. Temporary names are
unique per operation and failure cleanup removes only that operation's temporary file.

Each file is published separately. This does not create a transaction across
execution.json and state.json or promise directory persistence through sudden power
loss. Configuration and client-state saves still return false on failure;
ExecutionPersistence retains its existing void API and silent failure reporting.
The fix preserves an existing file, not data already lost before the upgrade.

## Preserve complete client-state strings

Client-state serialization previously discarded carriage returns. Commands, paths
and GPU names now retain carriage returns, newlines and literal backslashes when
saved and loaded again. Older files written by the client retain their established
escape behavior. Persistence tests do not execute saved commands.

## Unavailable process identities

A negative saved process start time could previously become a very large unsigned
number instead of an unavailable identity. Strict integer parsing rejects negative
or signed unsigned values, overflow and trailing text. An invalid or missing start
time remains zero, while the occupied assignment stays available to existing
conservative runtime checks. No PID is signalled as part of parsing these fields.

## JSON recovery independent of presentation

Recovery previously matched status text with a fixed spacing pattern and searched
for true or false in all text after echo_output. Minifying a valid JSON state could
hide FINISHED, and a later command containing true could override echo_output: false.
Recovery now reads complete JSON fields, independently of whitespace and field order.
Numeric values are parsed fully with the classic locale; invalid counters and
incomplete documents retain their established unavailable defaults.

## Validation

The six defects were reproduced against published 1.0.42 and committed together as
`0633d95e924fab0502cf6cb5d64e650d3dd548bb`. All 149 native client tests passed on
Ubuntu, including the desktop UI. The release build reruns the complete suite
against versioned source before creating packages.

Three new suites, ConfigurationNumericTests, ClientStateEncodingTests and
PersistenceFailureTests, raise the count from 146 to 149. Existing
ExecutionPersistenceRoundTripTests cover JSON spacing, field order, booleans and
invalid numeric values. The tests use temporary files and isolated child processes
with a file-size limit to induce short writes. They request no assignments and
launch no search engines. All 20 focused suites passed during local preparation.

## Upgrade

Allow active assignments to finish before replacing the installed client:

    openpuzzle safestop

Inspect identity warnings and wait for the relevant runtimes to become idle.
Then install the package:

    sudo apt install ./OpenPuzzle-1.0.43-Linux-x86_64.deb

Verify the installation and local configuration:

    openpuzzle --version
    openpuzzle doctor --offline
    openpuzzle status

Start searches explicitly using the established device and backend arguments.
The upgrade does not automatically start a search.

The portable updater package retains its established identity:

    OpenPuzzle-1.0.43-portable-XXXXXXXX.deb

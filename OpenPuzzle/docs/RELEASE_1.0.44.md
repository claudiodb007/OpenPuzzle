# OpenPuzzle 1.0.44 — Preserve a stable local client identity

OpenPuzzle 1.0.44 groups five reproduced local identity defects in one correction
commit. First callers agree on one complete UUID, failed creation does not leave
partial data, and existing damaged identities are preserved instead of silently
rotating the anonymous client identifier. Valid identities keep their original UUID.
All 150 native client tests passed before preparing this release identity.

## One identity across concurrent first starts

Two clients starting for the first time could previously read a missing client.id
and generate different UUIDs, even though only one would remain on disk. Reading
and first creation now share an exclusive process lock in the private configuration
directory. Cooperating callers re-read the identity under that lock and return the
same published UUID. The client.id.lock file remains in place after each call,
preventing another caller from locking a replacement inode during an operation.

The guarantee applies to clients that use this lock. An older running client does
not participate in this serialization. Existing valid UUIDs are not rotated.

## Reject incomplete or malformed identities

Loading previously accepted any non-empty first line, including a truncated UUID
left by a failed write. The complete identity must now have the established UUID
shape, with hexadecimal digits and separators in their expected positions.
Unexpected extra records are rejected. Existing UUIDs with no line ending, LF or
CRLF remain supported, preserving both their original bytes and uppercase or
lowercase hexadecimal text. An invalid identity produces a controlled failure
rather than being sent to registration as though it were valid.

## Preserve existing identity files

An existing empty file was previously treated like a missing identity and replaced
with a new UUID. Only an absent client.id now permits generation. Empty, malformed,
unreadable or non-regular identity files retain their contents. Identity and lock
symlinks are rejected without modifying their targets; FIFOs do not block the reader.

A damaged existing file is not repaired automatically. Preserve the file and recover
the original UUID from a known backup or a matching local assignment before retrying.
The runtime reports the identity path when loading or creation fails. This release
does not reconstruct an identity already lost before the upgrade.

## UUID formatting independent of locale

UUID generation previously inherited the global numeric stream locale. Regional
digit grouping could insert separators into hexadecimal byte values and produce
invalid UUID text. Generation now uses the classic locale and retains the established
UUID version 4 layout. No regional grouping appears in the stored identifier.

## Complete private publication after a successful write

First creation previously wrote directly to client.id. A short write could leave a
partial file that the next call would accept as an identity. Creation now uses the
existing atomic private writer: a unique temporary file in the same directory is
written completely, flushed and closed before being published by rename. A failed
write publishes no partial identity and removes only its temporary file. A later
successful call can retry first creation. Identity and lock files are owner-only.

This is publication of one file, not a transaction across configuration or assignment
files. It does not promise that directory entries survive a sudden power loss.

## Validation

The five defects were reproduced against published 1.0.43 and committed together as
`69879eda0345d81ebd388be3bea47b7573092ab7`. All 150 native client tests passed on
Ubuntu, including the desktop UI. The release build reruns the complete suite
against versioned source before creating packages.

ClientIdentityIntegrityTests raises the registered test count from 149 to 150.
It tests eight simultaneous first callers, an existing process lock, a real short
write induced in an isolated child, malformed or empty files, valid legacy line
endings and case, locale grouping, directories, FIFOs and symlink targets. All test
identities and lock files are in temporary directories. The tests request no
assignments, register no clients and launch no search engines.

## Upgrade

Allow active assignments to finish before replacing the installed client:

    openpuzzle safestop

Inspect identity warnings and wait for the relevant runtimes to become idle.
Then install the package:

    sudo apt install ./OpenPuzzle-1.0.44-Linux-x86_64.deb

Verify the installation and local configuration:

    openpuzzle --version
    openpuzzle doctor --offline
    openpuzzle status

Start searches explicitly using the established device and backend arguments.
The upgrade does not automatically start a search.

The portable updater package retains its established identity:

    OpenPuzzle-1.0.44-portable-XXXXXXXX.deb

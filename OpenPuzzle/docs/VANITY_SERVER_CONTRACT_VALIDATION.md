# Vanity receipt contract — isolated validation

Base: `a22ab52eefef04e340a10de936d24c5987aceca5`. Development 0.16.0. Report: `/home/claudiodb/OpenPuzzle/relatorios/vanity-server-contract/contract-47oz1leq`.

Three new Python suites passed in the user environment, with separate CTest
JUnit and source/interpreter/configuration provenance. Scope JUnit SHA-256:
`95b8e60d7063a44afb37e75f26bb10238e6044683adb8bf11f310284002d333b`. Prior 7/7, Linux 192/192 and platform 24/24
are separate immutable records and were not run again.

The SQLite reference atomically commits a single-assignment effect and its
immutable request-bound receipt. Duplicate UUIDs cannot apply twice; different
payloads conflict. Read-only queries classify recorded, refused and unknown
operations. Tests cover concurrent requests and an owned process killed before
and after commit, lost claim/completion replies, scope/lease/count mismatches.
The offline endpoint proposal performs no network I/O and remains blocked.

This is not a deployed server or a native runtime integration. PHP/MySQL API,
authentication, renewals, retention, deployment and release gates remain future
work. Existing ambiguous-owner cases stay blocked. Native source, common HTTP
client, local coordinator, UI, measured engines, adapter, profiles and prior
reports are unchanged. No target was compiled, no real GPU/benchmark run,
installation, publication or production request occurred. Production is disabled.

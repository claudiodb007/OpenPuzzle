# Vanity durable receipts — isolated reference contract

Base: `a22ab52eefef04e340a10de936d24c5987aceca5`. Development 0.16.0.

This stage adds an undeployed versioned protocol proposal and an SQLite reference
ledger, independent from the existing native local coordinator. There is no PHP
or MySQL server implementation in this context archive. Existing API endpoints,
authentication and deployment support have not been inspected or verified.
Production remains disabled. No server, client runtime, GUI, engine, adapter,
profile or previous run is migrated or changed by the reference implementation.

## Transaction and receipt contract

Each claim, progress or complete operation carries a canonical operation UUID,
client identity, protocol version, immutable scope digest and strict operation-specific
fields. Scope binds puzzle, range ID/bounds, target and required backend. A
receipt with a changed target/range/backend cannot confirm an operation. A SHA-256
of the canonical full request binds its immutable receipt. UUID reuse with a
different payload or client fails. A committed duplicate returns the identical
receipt, including after a completion's original lease has expired. Refused
operations are recorded too; changing a refused request requires a new UUID.

The reference commits the effect and receipt in one `BEGIN IMMEDIATE` transaction
with SQLite `synchronous=FULL` and a rollback journal. Concurrency cannot produce
two claims in its single-assignment scope. A killed process before commit leaves
neither effect nor receipt; a killed process after commit leaves both. This is a
local reference, not evidence of power-loss durability on all filesystems or of
MySQL implementation correctness. No range allocator or fairness policy changes.

Lease epoch, client and assignment fence updates. Lease validity is checked for
new effects; replay only retrieves the original immutable receipt. This small
reference has a fixed lease duration, no renewal, reassignment or lease epoch
rotation. Those production lifecycle operations remain future work. Canonical
decimal counts are bounded; progress cannot regress or exceed the local total.
A complete operation needs a previously accepted full count. This proves server
accounting only: native exact coverage and terminal proofs remain separately
mandatory before any client may claim completion.

Receipt queries use a read-only database connection. They bind the original full
request and return `recorded` plus its receipt, or `unknown`. Unknown is an
explicit review state and never authorizes another claim or automatic retry.
The pure `reconcile` classifier confirms only the recorded server outcome. It
does not launch work, send HTTP, modify checkpoints, release ownership, seal a
run or bypass native identity, coverage, lease or owner gates.

## Endpoint proposal and remaining integration

The offline manifest proposes these **new, unimplemented HTTP routes**:

| Role | Proposed route |
| --- | --- |
| Atomic operation execution | `/api/vanity/v1/operations` |
| Read-only exact receipt query | `/api/vanity/v1/receipts/query` |

These routes are not aliases for `/api/range/*`. No assumption is made that a
deployed server provides them. The manifest requires an explicit canonical origin,
HTTPS for remote origins, exact protocol/routes, deduplication, atomic receipts,
read-only queries and lease fencing. Credentials and extra fields are refused.
Even a valid manifest reports `readiness=blocked` and `production_enabled=false`:
declared support is not evidence of a reachable, authenticated implementation.
No DNS or connection occurs during preflight.

Before production: implement authenticated server operations and queries with
MySQL transaction/concurrency tests, define retention, renewals and epoch changes,
verify deployed version/capabilities, then integrate durable native operation
intent and receipt reconciliation without replaying claims or launching kernels
on an ambiguous outcome. Existing 0.15 ambiguous-owner cases remain blocked.

## Validation scope

Only three new Python suites are required: ledger transactions/deduplication and
concurrency, actual owned server-process loss plus strict receipt recovery, and
offline endpoint preflight. All operations are synthetic and local. Preparation
configures CMake to register these suites; it compiles no target and re-runs none
of the prior seven suites. Prior 7/7, Linux 192/192 and platform 24/24 evidence,
binaries, measured profiles and real GPU UI validation remain immutable records.

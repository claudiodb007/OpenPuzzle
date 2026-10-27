# Native durable receipt integration — local opt-in only

Base: `3dc2a68d10d3aca2354b592cdea303f1e7bd7281`. Development 0.17.0.

The actual native RunSession local route can now use the isolated 0.16 receipt
protocol through the explicit option
`--receipt-protocol openpuzzle_vanity_receipts_v1`. All existing local binding
options, `run 20`, Vanity/CUDA selection, measured executable hashes, GPU UUID,
isolated HOME and experimental local opt-in remain mandatory. Only numeric
`http://127.0.0.1:PORT` is accepted. The new flag never falls through to the
ordinary runtime. Desktop session actions refuse it; no existing UI/store is
migrated and there is no production path or fallback to the legacy range API.

## Operation intent and confirmation

Native `receipt-client.json` is an owner-only, atomic file with a directory
fsync. It binds the CLI/session fingerprint, endpoint, client, exact scope and
ordered operation history. UUIDs use Linux kernel entropy. Claim, progress and
complete intents carry the canonical full request and its SHA-256, durably
published **before sending any operation bytes**. Receipts must match the UUID,
client, kind, digest, assignment, lease epoch, count and exact puzzle/range/
target/backend scope. Confirmation is saved durably before advancing the
native coordinator. Journal size/operation limits fail closed.

A failed, malformed or lost operation reply leaves the original intent in place.
Bounded legacy retry loops cannot resend it or create another operation. The
invocation stops; it does not automatically query or recover. Explicit `--resume`
uses the read-only receipt query for the exact last operation, even when a
receipt was already saved before a process loss. Cached receipt disagreement,
unknown result, unavailable peer, wrong identity/version/type/scope, missing
journal and changed intent/digest remain blocked. No force or unknown-result
replay switch is provided.

A recorded claim is adopted as the same assignment. Resume performs a fresh
progress/lease probe before launching/relaunching work; an old immutable receipt
alone cannot renew or prove a current lease. A recorded completion is adopted
without sending another completion, progress or starting a kernel. Existing
native segment checkpoints and exact contiguous engine/terminal evidence remain
mandatory: a server receipt cannot replace native coverage proof. Stop, Safe
Stop, per-batch durable counters and HTTP progress pacing are retained. Completion
and claim operations are never replayed to the server during reconciliation.

The typed bounded receipt JSON reader rejects duplicate keys (including escaped
aliases), extra fields, boolean/integer confusion, floats, overflow, excessive
nesting, trailing content and unsupported strings. Operation serialization
matches the strict ASCII contract's canonical sorted-key JSON.

## Isolated peer and limits

`ReceiptCoordinator.ReceiptAssignmentServer` exposes the proposal's two loopback
routes and wraps the existing SQLite reference ledger. Effects and receipts
remain one durable database transaction, and receipt lookup uses a read-only
connection. Existing registration/heartbeat fixtures are retained; legacy
`/api/range/*` mutation paths are refused by this peer. Fault injection exists
only in the tests. A restarted peer can reuse its original endpoint/client and
ledger to answer the native client's outstanding operation.

This route is separate from the owned desktop helper's legacy `ACTIVE`/seal
workflow. It does not make earlier ambiguous desktop cases recoverable, change
their seals, add a UI action or enable real GPU/production execution in this
preparation. The reference remains one synthetic assignment in puzzle 20 with
range ID 37, a fixed lease, no renewal/epoch rotation/retention/reassignment and
no authentication. A discovered solution is exported privately by the existing
native exporter, stops the run and remains for explicit review; this protocol
does not send a solution notice or a completion.

No PHP/MySQL implementation or deployed API support has been verified. The
0.16 offline manifest still reports blocked readiness and disabled production.
Before remote integration: obtain the actual server sources, implement and test
authenticated atomic MySQL receipts and lifecycle operations, verify the
deployed contract and complete release gates. A later local step can connect
this receipt path to the owned desktop helper with equivalent snapshot/seal
and owner-loss evidence gates. Existing profiles, engines and prior reports
remain unchanged.

## Validation

Eight affected suites: native receipt integration, strict receipt JSON, existing
native coordinator, HTTP preview, desktop Stop/Resume, network recovery,
owner-loss recovery and strict public RunSession routing. Tests use the actual
native CLI/adapter with synthetic engines/inventory; process loss targets only
the recorded owned test child at a controlled HTTP boundary. They cover lost
claim/progress/completion replies, peer restart/unavailability, exact intent
before HTTP, strict receipt validation, refused leases, changed/missing journals,
native coverage independent of server completion, Stop/Resume, Safe Stop and
private solution export. No benchmark or real GPU execution occurs.

Preparation compiles CLI/adapter plus the route and JSON test targets, not the
interface or measured engines. Each affected suite has its own JUnit/provenance
and can be reused on an unchanged retry. Prior 3/3 receipt contract, earlier
7/7, Linux 192/192 and isolated platform 24/24 remain separate immutable evidence;
no new complete Linux/platform run is claimed. Commit is an explicit separate
step; no installation, publishing or production request is performed.

# Owned desktop Vanity sessions with durable local receipts

Development 0.18.0 connects the receipt protocol to the existing public
RunSession commands and UI controls: Start, Stop, reopen/Resume and Safe Stop.
The UI and measured engines are unchanged. A new private preview store opts in
through `coordinator-receipts.json`, bound to its selected measured profile,
CLI and helper sources. Existing stores and historical runs are not migrated.
Removing or changing this configuration cannot downgrade an existing receipt
run to the legacy coordinator. Production remains disabled.

Each owned run has its own native receipt journal and isolated SQLite peer.
The peer stores effects and receipts atomically. The native client persists an
operation intention before HTTP and queries its exact receipt on explicit
resume. Neither a claim nor a completion is replayed. A receipt does not replace
the independent native checkpoint and exact contiguous segment terminal proof.
Solutions stay private and require review; no solution notification is sent.

Listing and recovery proposals are read-only. The snapshot binds the private
files, configuration, seal or ACTIVE head, native coverage, operation identities,
receipt history and peer assignment. Resume requires the exact displayed token.
Old evidence is archived before a confirmed mutation; checkpoints are never
edited by recovery. Missing or changed evidence, an unknown receipt, extra peer
operations, a SQLite recovery sidecar or a refused lease blocks recovery. No
automatic SQLite transaction repair is performed during inspection.

After helper/native process loss, recovery requires both recorded process
identities to be verifiably dead and all native ownership locks to be free.
Ambiguous identities and live owners block recovery. An interrupted HTTP
request whose receipt is already durable is archived unchanged and represented
by an explicit local 599 `receipt_resolved_without_delivery` event. This event
does not claim delivery of a response. The native client still queries the
exact receipt independently before advancing.

If native coverage is already complete, receipt reconciliation skips GPU
inventory and telemetry and launches no new kernel. A durable native done
outcome can be adopted without a process or socket. No new claim, progress or
completion is sent for an already applied completion.

The local reference provides one assignment and a fixed 300-second lease;
expiry remains blocking. Lease renewal, reassignment, production endpoint
authentication and the PHP/MySQL implementation are outside this stage.
Server sources must be inspected before any remote integration or deployment.

Validation covers six affected suites with synthetic engines/inventory:
legacy desktop/recovery/owner loss, native receipts, receipt desktop and receipt
owner loss. Historical 8/8, 3/3, 7/7, Linux 192/192 and platform 24/24 remain
separate preserved evidence. Preparation rebuilds only CLI/adapter to bind the
new helper path, without rebuilding the UI or engines. No benchmark, real GPU
search, installation, publication or production request is part of preparation.

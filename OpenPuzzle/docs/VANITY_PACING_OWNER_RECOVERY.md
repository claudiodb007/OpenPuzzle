# Vanity progress pacing and explicit recovery after owner loss

Base: `fd9b6eb291a283e8d29766e9038f24efecadd90a`. Local development kit: 0.15.0.

The native coordinator still persists local checked progress for every batch,
but coalesces routine HTTP progress with a monotonic five-second deadline. The
first positive progress, lease probes before launches, segment/final boundaries
and a final durable Stop count bypass that deadline. An acknowledgement alone
advances the pacer. A pending completion continues to require exact engine
terminal proof; progress at 100% is insufficient. Heartbeat cadence is retained.

The helper reads local native progress for UI updates, so the displayed counter
does not wait for a paced HTTP request. Engine ranges, per-batch checkpoints,
Stop, Safe Stop and ordinary runtime behavior remain intact.

The owned local server now maintains `http-live.json`. It durably publishes an
intent before applying a request, then publishes the response and observed
effects before returning the reply. Each publication updates the active
transaction's journal hash before acknowledging the peer. An incomplete intent
or a journal/head mismatch stays review-required; it is never assumed committed
or uncommitted after a crash.

The active record stores the helper/native process identities: local and proc
PIDs, start ticks, boot ID, PID namespace and proc view. pidfd metadata resolves
PID namespace differences; a numeric fallback is permitted only in the same
PID view. Unknown or live processes prevent recovery. Both the coordinator and
segment owner locks must be free before inspecting a recovery candidate.

After abrupt loss, listing is read-only and offers a snapshot token only when
source/profile/binary bindings, settled peer effects and exact durable native
coverage agree. Resume requires that exact current token, archives the original
active transaction and evidence, rebinds the original loopback port/client and
continues the same assignment. Native cached progress is reconciled to verified
adapter coverage before the lease probe. No checkpoint is manually rewritten.

If the native run already has a confirmed done outcome and one applied
completion, explicit recovery can finish the seal without another process, GPU
check or RPC. Lease rejection, live/unknown owners, review checkpoints, changed
journals, stale confirmation, pending claims and applied-but-unconfirmed
completions remain blocked. An unrecorded spawn or a crash between journal and
head publication still requires review. There is no force-recovery switch.

New metadata uses `openpuzzle_coordinator_desktop_v4`, pinning OwnerRecovery.py.
Prior stores and seals are not migrated. A separate worktree and store preserve
the prior 0.14.0 commit, binaries, reports and earlier real GPU UI validation.
The UI layout, ordinary runtime, common HTTP client, measured engine and adapter
source are unchanged. Production Vanity remains disabled.

Seven suites cover this scope: local contract (including write-ahead effects),
native coordinator, desktop Stop/Resume, network/lease recovery, strict routing,
the pacer with a deterministic clock and actual owned process-loss scenarios.
All engine/inventory runs are synthetic; there is no benchmark or real GPU run.
Preparation compiles only CLI/adapter, route-test and pacer-test targets, then
runs those seven suites. The prior 5/5 scope, Linux 192/192 and platform 24/24
JUnit remain separate immutable evidence; no new full-suite run is claimed.

Further work remains before production: durable server reconciliation for
ambiguous transactions, explicit endpoint configuration and production/release
gates. This stage does not install, publish or enable production.

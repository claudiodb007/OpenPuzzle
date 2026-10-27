# Vanity progress pacing and lost-owner recovery — local scope validation

Base: `fd9b6eb291a283e8d29766e9038f24efecadd90a`. Development kit: 0.15.0.

The native client keeps local per-batch progress but coalesces routine HTTP
progress with a monotonic five-second deadline. First positive progress, lease
probes, segment/final boundaries and Stop flushes remain mandatory. The helper
uses local native progress for UI updates. Exact range/terminal checks remain.

The local peer durably records request intent/effects and updates the active
journal hash before responding. Explicit lost-owner recovery requires dead,
known helper/native identities, free coordinator/segment locks, a settled and
unchanged journal, exact durable coverage and a current snapshot confirmation.
PID namespace/proc-view identity is resolved with pidfd metadata. Unknown/live
owners, partial journal/head publication and ambiguous requests stay for review.
Confirmed done adoption starts no process and sends no new RPC. No checkpoint
is manually rewritten, and the same assignment is retained without a new claim.

Seven suites passed in the actual user build: the five previous affected suites,
VanityCoordinatorPacerTests and VanityOwnerRecoveryTests. Tests use a deterministic
clock and actual owned process loss with synthetic inventory/engine only.
Scope JUnit SHA-256: `6fb8f373760692e061188d0158e09b956b5d1c9375d7c0c08884cbaa9275a0e5`.
Report: `/home/claudiodb/OpenPuzzle/relatorios/vanity-pacing/pacing-z7fiefxm`.
The prior 5/5 scope, Linux 192/192, isolated platform 24/24 and real GPU UI evidence
remain separate immutable records. No full-suite rerun or new real GPU test is
claimed for this stage.

Metadata uses schema v4. Prior stores/seals are not migrated. UI code, ordinary
runtime, common HTTP client, measured engine and adapter source remain unchanged.
Preparation rebuilds CLI/adapter and route/pacer test targets only. Production
stays disabled; no benchmark, installation or publication was performed.

Next: durable server reconciliation and explicit endpoint/production preflight.

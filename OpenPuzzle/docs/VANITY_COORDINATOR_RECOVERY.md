# Local Vanity coordinator: explicit network and lease recovery

Base: `e521a2df42f122f0724ea621757b21a82088cde4`. Development kit: 0.14.0.

This stage extends the existing local RunSession path. The main window's Resume
control remains an explicit, seal-confirmed action on the same assignment.
There is no production endpoint or automatic new claim.

The native coordinator publishes `coordinator-outcome.txt`, bound to its native
session binding, phase, durable counter, launch count, process and fresh
invocation token. A stale outcome cannot validate another process invocation.
After a failed in-flight progress upload, it reconciles its local checked count
to the adapter's durable checkpoint and independent exact batch evidence.

The local HTTP server records request/response effects separately from requests.
The helper distinguishes local checked keys from acknowledged progress and
counts applied completions, rather than counting unsuccessful completion RPCs.
Both journals, the native outcome and coverage must agree before a new seal is
published. The outcome includes the actual exit code and is pinned by the seal.

| Observed result | Explicit Resume behavior |
| --- | --- |
| Temporary progress/heartbeat failure after an unambiguous claim | Probe the same assignment and continue from exact durable coverage |
| Confirmed transient completion failure, with all kernels already complete | Retry completion without another kernel or claim |
| Lease/assignment rejected, including rejection at completion | Keep a terminal sealed record; do not resume or claim replacement work |
| Lost claim reply | Keep the active transaction for review; do not claim again |
| Lost completion reply or an already-applied ambiguous completion | Keep for review; do not immediately replay completion |
| Permanent error, false acknowledgement, missing terminal proof, altered journal or stale outcome | Keep for review; do not automatically resume |

HTTP retries remain bounded: at most three attempts with 100/250 ms backoff for
temporary failures. A lost completion reply, permanent error, malformed success
or lease rejection stops retries. This is not production-scale progress pacing;
normal per-batch reporting and the existing heartbeat cadence are retained.

Metadata now uses `openpuzzle_coordinator_desktop_v3`, including the recovery
helper's hash; the owned seal uses `openpuzzle_coordinator_seal_v2`. Prior stores
and seals are not migrated. The preparation uses a separate worktree/store and
preserves the 0.13.0 commit, binaries, reports, bytecode correction and previous
main-window real GPU evidence. No UI layout, ordinary runtime, common HTTP client,
measured engine or adapter source is changed.

Five suites cover this stage: local contract, native coordinator, desktop
stop/resume, new recovery scenarios, and strict RunSession routing. Tests use
real native RunSession/HTTP/adapter code with synthetic GPU inventory and engine.
The recovery cases cover outages before/during kernels, heartbeat failure,
completion retry with no kernel, lease rejection, lost claim/completion replies,
permanent errors, false acknowledgements, altered journals, stale invocation
outcomes and the public RunSession resume path.

The user preparation compiles the CLI/adapter and route-test target, and runs
only these five suites. Their JUnit stays separate from the earlier 4/4 scope,
Linux 192/192 and isolated-platform 24/24 records. Preparation runs no real GPU
search or benchmark, rebuilds no UI, installs nothing and creates no commit.
Explicit consolidation reuses the receipts without compiling or running tests.

An abrupt helper/owner crash still leaves an active transaction requiring review.
Safe recovery from that class, production-scale pacing and endpoint/release
configuration remain future work. Production Vanity remains disabled.

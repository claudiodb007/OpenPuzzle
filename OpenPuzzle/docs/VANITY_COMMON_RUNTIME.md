# Vanity in the common engine and execution components

Base client: `8405bfbc7394184796a7c5735c06653a1c77febf`.
Development scope: 0.19.0. No production routing, installation or server change.

The uploaded PHP API snapshot contains the existing range claim/progress/complete
handlers, dispatcher, lease service and coverage validation. Linear claims do
not require a separate engine-specific endpoint. The dispatcher reuses an
existing eligible assignment for the same client/puzzle/execution slot.
Progress validates identity, monotonic bounded coverage and the current lease,
then renews it for 15 minutes in the existing transaction. Completion checks
full coverage and accepts a repeated equivalent terminal state. The source
snapshot is evidence of code, not proof of the currently deployed version;
no live PHP/MySQL execution or production request was performed.

This change registers the already validated native Vanity adapter through an
explicit `EngineManager(VanityEngineConfiguration)`. The default manager keeps
the existing three engines. A verified profile supplies absolute executable
paths and SHA-256 bindings; no new engine discovery or benchmark is introduced.
The existing `ExecutionRequestBuilder` creates a Vanity execution request, the
existing `BackgroundExecutionLauncher` owns its process and `ClientStateStore`
stores it. `EngineParserFactory` and `ExecutionSyncService` read its public
progress and synchronize with the existing `HttpRangeClient` endpoints. No
Python coordinator, local receipt peer or additional network loop is used in
this common-component path.

The native adapter gains an optional common-runtime frame and parent ownership
binding for the ordinary background supervisor. Existing preview invocations
retain their protocol and owner contract. Native checkpoints, exact contiguous
batch/attempt proof and terminal records remain mandatory. A shared terminal
log or acknowledgement cannot replace that proof. Changes to executables,
assignment binding, checkpoint/attempt evidence or protocol tails block
acceptance. Progress exceeding native coverage is not uploaded. Old log
generations are preserved on adapter resume. Private results use the existing
protected result format and wallet exporter; public protocol lines carry no
private key. Completion retries use the existing idempotent complete endpoint
and do not relaunch the engine.

The shared adapter deliberately retains the existing scalar-width and count
limits: `[2^64,2^80)`, at most `2^40` keys, validated power-of-two grid and a
bounded evidence budget of 8192 batches. It accepts one target, compressed
CUDA search and logical device zero. These are capabilities for the next common
preflight, not permission to send an incompatible production assignment.

The public production `RunSession` gate and existing UI selection still route
Vanity to the preserved preview. This phase validates the common engine,
launcher, state and synchronization connection before switching that route.
Next: wire the verified measured profile and these bounds into ordinary
preflight/claim sizing, recovery and the existing UI controls, then validate
that public route. Reuse the existing dispatcher/lease/finalization behavior;
do not introduce a Vanity scheduler or receipt API. No server schema or PHP
modification is currently required by this component contract.

Affected validation: EngineManagerTests, EngineParserFactoryTests,
ExecutionRequestBuilderTests, ExecutionSyncServiceTests, VanityPreviewModelTests,
VanityCommonRuntimeTests, VanityNativeReceiptTests and VanityReceiptDesktopTests.
Synthetic engine/inventory only; historical suites and GPU evidence remain
separate, preserved results.

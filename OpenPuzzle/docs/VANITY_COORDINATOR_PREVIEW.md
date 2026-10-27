# Native Vanity coordinator contract — preview 0.11.0

This Linux-only stage validates the coordinator contract before enabling Vanity
in the normal production run path. The existing desktop interface is unchanged.
`run --engine vanity` remains blocked. The explicit `vanity-preview
coordinator-test` command accepts only an owned numeric-loopback endpoint and a
synthetic Puzzle 20 scope; it cannot contact the official coordinator.

## Reused client and runtime

The session uses `client::HttpRangeClient` for registration, heartbeat, claim,
progress, completion and solution-report payloads/parsing. A per-instance
transport overload allows the existing bounded numeric-loopback transport to
carry those normal requests without DNS, proxies or redirects. The original
one-argument constructor and ordinary HTTP path remain available and unchanged.
`ClientRuntime` handles exact completion and the existing protected solution
export/report flow. The native engine registry/factory launches the bound Vanity
adapter. All measured engine/adapter binaries and the candidate profile remain
unchanged; this stage does not attach a new benchmark rate to rebuilt code.

## Server-assigned ranges

The coordinator supplies the assignment ID, range ID and interval. The plan
checks puzzle, target, mode, CUDA backend, scalar width and local metadata scope
before launching. The whole interval is split into consecutive segments of at
most 2^40 keys, matching the unchanged measured adapter's limit. Tail coverage is
exact. Arithmetic tests include a 2^44 + 17 key assignment; no such large GPU
search is performed by the automated tests.

One assignment ID and cumulative keys_checked span all segments. A new segment
does not issue a new claim. Every relaunch probes the existing lease; a rejected
lease blocks further launch and completion. Transient progress failures preserve
state. Safe Stop finishes the entire current assignment across its segments.
Cancelled runs retain the assigned lease and require an explicit, successful
resume probe; they do not claim a replacement. Ambiguous claims stay blocked.

The private state and binding record pin server, client ID, scope, public target,
physical GPU, engine/adapter fingerprints, geometry, slot and coordinator CLI
fingerprint. Native checkpoints and protocol logs must prove a contiguous
prefix. Completed segment evidence is rechecked before completion/recovery.
Pending completion can be retried without relaunching a completed segment.
The terminal state remains preserved and blocks another claim in this session.

## Validation and remaining integration

Native session fixtures cover three-segment exact tails, unavailable work,
ambiguous claims, wrong target/scope, progress/completion retries, lease rejection,
cancel/resume, owner crash/recovery, altered configuration/checkpoints, Safe Stop,
missing acknowledgement, redirect refusal and protected solution export. Ordinary
HTTP-client tests and the earlier HTTP preview regression also run locally.

The preparation kit expects 189 full Linux suites and 24 isolated platform
suites on the user's machine. It never runs a real GPU by default. A separate
explicit `--run` checks one absent-target local assignment of 2^36 keys split in
two, using the exact previously measured engine/adapter, unchanged power/fans,
physical identity checks and the existing 85 °C guard.

Still pending: real GPU confirmation of this coordinator session, integration
into the ordinary RunSession/status/stop lifecycle and the existing UI controls,
production registration/lease operation, production progress cadence/performance,
broader hardware/Windows validation and release packaging. The fast per-batch
progress cadence is for this bounded fixture. This is preparation for 1.0.49,
not a release or authorization to launch a production assignment.

# Vanity through ordinary execution and main controls (0.20.0)

Base: fe80d64bf574fe104e37d299dd2156947332c175. The measured native engine,
its original evidence and the existing BitCrack/Kangaroo/KeyHunt routes are reused.

The optional Linux preview now passes an owner-only `OPENPUZZLE_VANITY_COMMON_PROFILE`
into the ordinary CLI and main window. The profile binds the new CLI/adapter hashes,
original measured engine, complete measurement/evidence pins, physical GPU UUID,
driver, power context and measured grid/threads/points. Its preparation reads the
existing evidence without launching the GPU or repeating a measurement.

With the bound profile, the existing selector generates ordinary
`run <puzzle> --engine vanity --backend cuda --device <physical-index>` arguments.
The same Start, Stop, Safe Stop, status and detached runtime are used. A stopped
Vanity assignment is shown as resumable in the ordinary main window. Benchmark and
automatic-start controls are unavailable for this acceptance mode. Existing local
preview commands remain compatible when explicitly selected.

Preflight requires CUDA, one physical device matching the measured UUID, an idle
GPU, matching driver/power/geometry, and an explicit linear puzzle in the supported
65–80 range with bounds inside [2^64,2^80). The existing PHP claim payload gets its
already-supported optional `requested_keys`, capped by the adapter and evidence
budget. Other engines retain their original payload. The returned assignment must
match the validated puzzle/target/backend/bounds before launch.

The common workspace seals its profile and server URL. Stop preserves that state
and the native contiguous checkpoint instead of cancelling/reclaiming the lease.
Resume validates the same assignment, binaries and independent native attempt
coverage, then probes the existing progress endpoint before launching the remaining
range. A refused lease blocks execution and preserves evidence. Owner loss can
rejoin an already-active engine without another launch. Unknown process identity
cannot authorize replacement.

A completed native proof can retry the existing idempotent completion endpoint
without a kernel or a progress probe: after a lost HTTP reply the server may have
already finalized the lease. No new claim, receipt protocol or coordinator is
introduced. A partial or corrupt terminal proof remains preserved for review.
Private solutions continue through the common solution/export path.

This development acceptance gate permits only an explicit numeric loopback HTTP
server. Loopback requests use the existing HttpRangeClient with proxies and
redirects disabled. Remote/production execution remains blocked. The reviewed PHP
source is unchanged; deployed version parity and PHP/MySQL execution are not
claimed. Real GPU validation of the ordinary route is a separate optional acceptance
step, not another benchmark. Existing prior GPU evidence remains preserved.

The preparer builds CLI, adapter and main UI, and checks thirteen affected suites.
Previous Linux 192/192, isolated platform 24/24 and phase-specific results are
preserved separately. No installation, publication, production HTTP or GPU work
occurs during preparation or consolidation. Kangaroo checkpoint storage/retention
is pending after the Vanity integration.

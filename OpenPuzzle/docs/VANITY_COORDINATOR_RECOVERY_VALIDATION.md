# Vanity coordinator network/lease recovery — local scope validation

Base: `e521a2df42f122f0724ea621757b21a82088cde4`. Development kit: 0.14.0.

The native loopback coordinator now publishes a durable outcome bound to its
native binding and a fresh invocation token. Recoverable progress failures are
reconciled only to the adapter's durable exact batch coverage. HTTP retries are
bounded (three attempts, 100/250 ms backoff); malformed acknowledgements,
permanent failures and assignment/lease rejections are not transient retries.

The owned helper cross-checks request/response effects, local checked coverage,
acknowledged progress and the native exit. Explicit Resume continues the same
assignment after a temporary progress/heartbeat failure. Confirmed transient
completion failure can be retried after all work is complete, with no new kernel.
Lease/assignment rejection stays terminal. Ambiguous claim/completion replies,
review checkpoints, altered evidence and stale invocation outcomes stay blocked.
A lost completion reply is not immediately replayed by the native client.

Five affected suites passed in the actual user build: VanityLocalCoordinatorTests,
VanityCoordinatorSessionTests, VanityCoordinatorDesktopTests,
VanityCoordinatorRecoveryTests and VanityRunSessionRouteTests.
Scope JUnit SHA-256: `24e416579dcfbb11cb6c30d6c353fa4098070d7605d10ebfdbc53cf2196997f1`.
Report: `/home/claudiodb/OpenPuzzle/relatorios/vanity-recovery/recovery-q0a8hjnr`.
Previous 4/4 scope, Linux 192/192, isolated-platform 24/24 and main-window real GPU
controls evidence remain separate immutable records. No new full-suite run or
real GPU validation is claimed for this stage.

The main-window code, ordinary runtime, common HTTP client, measured engine and
adapter source are preserved. Local metadata uses schema v3 and a v2 seal. Prior
stores and seals are not migrated. Production remains disabled; there is no
continuous claim loop, production endpoint, benchmark, installation or publication.

Next: production-scale progress pacing and explicit recovery from abrupt owner
loss, followed by endpoint configuration and the remaining production gates.

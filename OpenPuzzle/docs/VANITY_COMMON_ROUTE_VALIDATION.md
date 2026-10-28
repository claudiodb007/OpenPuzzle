# Vanity ordinary run and main controls — affected-scope validation

Base: `fe80d64bf574fe104e37d299dd2156947332c175`. Development 0.20.2. Report: `/home/claudiodb/OpenPuzzle/relatorios/vanity-common-route/route-besgf3xc`.
Fourteen affected suites passed; compatible 0.20 results retain original source/binary/JUnit provenance, with only the new UI driver/suite built/run during that upgrade.
Scope JUnit: `b7c1749192b0b1aad83687654149c12b580f55e099b791c77b7ffecfc7fc3979`. Previous 8/8, 6/6, 8/8, 3/3,
7/7, Linux 192/192 and platform 24/24 are separate preserved evidence.

Vanity uses ordinary run/status/stop/safestop, the existing main UI controls,
EngineManager, ExecutionRequestBuilder, BackgroundExecutionLauncher,
ClientStateStore, ClientRuntime, HttpRangeClient and ExecutionSyncService.
An owner-only profile pins the existing full measurement evidence, new CLI/adapter,
original measured native engine, physical GPU UUID/driver/power and geometry.
No benchmark, engine rebuild, new coordinator or server API/schema change.

Existing claim requested_keys caps assignment size to measured adapter bounds.
Stop retains the same state/checkpoint and does not cancel/reclaim the assignment.
Explicit resume validates the profile/server/native evidence and existing lease.
Native completion retries through the existing idempotent complete endpoint without
relaunching a kernel or probing an already-finalized lease. Temporary process
identity failures remain blocked. Solutions use the existing private export path.

Tests cover ordinary start/completion, Stop/reopen/resume, Safe Stop, owner loss,
lost completion reply, lease refusal, unchanged native attempts, profile evidence,
GPU/geometry/range refusal, shared UI commands in four languages, and regressions.
Actual MainWindow-to-native-CLI acceptance also closes/reopens the UI and local peer, resumes the same native checkpoint, and finishes with Safe Stop. The optional report launcher opens this existing main UI, using the same local journal and bound profile. No alternate execution coordinator.
The bound run profile is `45103b129fd53b7bfa6fa79070fa4f7440c3bf751545094c929c6126973569f4`.

Only explicit numeric loopback HTTP is enabled for this acceptance phase; shared
loopback requests bypass proxies and do not follow redirects. Production is blocked.
Reviewed API source: `f1c5e1728a78b6a302d5fdbf8cd6127f289ddc25bbb5ab7199d07acc44de58c0`; no deployed version parity,
PHP/MySQL runtime, real GPU acceptance or release installation is claimed here.
No installation, publication, new measurement or production request.
Next: optional local one-assignment GPU acceptance of this ordinary route, followed
by deployed API parity before production. Kangaroo checkpoint storage remains next.

# Local Vanity runtime — scope validation

Base: `2658bce08b2ffa7fc289b6d5eb64e837a2d03d04`. Development kit: 0.13.0.

The local HTTP assignment server and explicit native argument construction now
live in measured/LocalCoordinator.py. The owned desktop coordinator no longer
imports a test fixture or modifies sys.argv while loading its contract.

Four affected suites passed in the actual user build: VanityLocalCoordinatorTests,
VanityCoordinatorSessionTests, VanityCoordinatorDesktopTests and VanityRunSessionRouteTests.
The standalone scenario starts, stops and resumes with no test tree, and rejects
an altered runtime contract before another engine launch. New metadata pins the
runtime contract (schema v2); previous stores and seals are not migrated.

Scope JUnit SHA-256: `1d7c03a8036b4d4867ec222f58e26b144486a84f669b08491fe9d8fd93f77e98`.
Report: `/home/claudiodb/OpenPuzzle/relatorios/vanity-local-runtime/runtime-9xwlewhf`.
Previous Linux 192/192 and isolated-platform 24/24 JUnit remain separate immutable
evidence. No new full-suite run or real GPU validation is claimed for this stage.
The prior main-window GPU controls/resume evidence remains in commit 2658bce.

Native C++ sources, main-window code, measured engines and profile are preserved.
Preparation compiled the CLI/adapter and route-test target to bind the new helper
paths; the previous interface binary was not rebuilt. Production remains disabled.
No real GPU run, benchmark, installation or publication was performed.

Next: coordinator pacing and explicit network/lease recovery before production.

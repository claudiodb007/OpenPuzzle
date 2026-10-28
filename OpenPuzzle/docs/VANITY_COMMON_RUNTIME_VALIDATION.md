# Vanity common native components — affected-scope validation

Base: `8405bfbc7394184796a7c5735c06653a1c77febf`. Development 0.19.0. Report: `/home/claudiodb/OpenPuzzle/relatorios/vanity-common-runtime/common-kgdxi0_1`.
Eight affected suites passed in the actual user CLI/adapter/common-component
build with separate JUnit/provenance. Scope JUnit: `493928c62e07631d63c9cef2698cb6d801269336f439f81b706075ba5f0e066b`.
Previous 6/6, 8/8, 3/3, 7/7, Linux 192/192 and platform 24/24 remain separate
preserved evidence. No full suite or GPU/benchmark was repeated.

The existing EngineManager, ExecutionRequestBuilder, BackgroundExecutionLauncher,
ClientStateStore, EngineParserFactory, HttpRangeClient and ExecutionSyncService
were used with the verified native Vanity adapter. Tests validate real common
claim/progress/completion payloads against an isolated peer matching the reviewed
PHP contract, private export, native interruption/resume and idempotent completion
retry without engine relaunch. Shared completion and progress cannot exceed the
independent exact native checkpoint/attempt proof. Existing preview behavior and
other engines remain covered by affected regression tests.

Reviewed API source manifest: `f1c5e1728a78b6a302d5fdbf8cd6127f289ddc25bbb5ab7199d07acc44de58c0`. Source only;
no deployed capability or PHP/MySQL execution is claimed. No server code/schema
change. Native engine and previous UI binaries/stores remain preserved. CLI,
adapter and affected test executables were built; no UI/engine rebuild,
installation, publication or production request.

Ordinary public RunSession routing is still blocked for Vanity. Next is wiring
verified measured profile and native bounds into ordinary preflight, claim sizing,
recovery and UI controls, reusing the existing dispatcher and HTTP API. See
VANITY_COMMON_RUNTIME.md. Production remains disabled.

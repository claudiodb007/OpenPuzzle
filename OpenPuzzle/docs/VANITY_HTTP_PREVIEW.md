# Vanity public targets and local HTTP preview 0.6.0

This extends the isolated CLI preview, not `run --engine vanity`. Production
selection remains blocked before recovery, identity creation or network access.
The UI and installed packages are unchanged.

## Public target engine

The new engine identifies itself as `OpenPuzzle-VanityRange 0.2.0 CUDA target
protocol 1`. Its search route accepts a bounded `OPV-TARGETS-1` text file with
`p2pkh ADDRESS` or `hash160 HEX40` records. Only compressed mainnet P2PKH is
supported. Base58Check, canonical encoding, target count, duplicate hashes and
input size are validated. No private scalar is supplied to this search route.
`--target-info FILE` and the existing fixture `--reference HEX` are host-only.

The 38 vendor files, including exact-range CUDA kernels, are unchanged from the
validated range preview. The new host converts public targets into the same
lookup representation. It CPU-verifies recovered points and writes durable,
owner-only `MATCH key hash address WIF` records before acknowledging progress.
No private result is printed in the progress protocol or sent over HTTP.

Compilation targets sm_89 / RTX 4070 SUPER only in this development kit. Other
GPUs and Windows are not validated by it. Existing measured profiles remain
bound to their original engine and adapter; no performance result is transferred
to the rebuilt executable.

## Local HTTP route

`openpuzzle vanity-preview http-test` requires all of:

* `--engine`, `--engine-sha256`, `--adapter`, `--adapter-sha256`
* `--gpu-uuid` matching `CUDA_VISIBLE_DEVICES`
* absolute `--root`, `--controls`
* `--server http://127.0.0.1:PORT` (1024–65535)
* `--start`, `--count`, `--grid`, `--target-kind p2pkh|hash160`, `--target`
* `--enable-experimental-vanity-local`

All parameters enter the workspace binding. The endpoint has no DNS, proxy,
redirect, TLS or external-host support. It uses bounded nonblocking sockets with
a three-second deadline per request; only Content-Length replies are accepted.
The small fixture envelope is deliberately stricter than the production API.
It uses the actual OpenPuzzle claim parser after structural validation and the
actual minimal solution-report builder. This does **not** validate production
transport compatibility or authentication.

One pinned Puzzle-20 assignment is permitted per workspace. The received target,
backend and exact interval must match the command. A claim is durably recorded
as pending before transmission; an ambiguous claim blocks automatic retry and
requires review. Before first launch/resume the server must accept progress for
the saved prefix. Rejection prevents another launch/claim. Temporary progress
failure stops the owned child and preserves its checkpoint.

Completion requires native exact coverage and terminal success. A lost reply
can retry completion without rerunning the engine. Restart of a pending
completion does not first send a running progress update for an already completed
assignment. Completion and solution-report endpoints in the fixture are
idempotent; this is an explicit requirement for later production integration.

The EngineRegistry, EngineFactory, process runner, native checkpoint adapter,
progress parser, ClientRuntime terminal monitor and SolutionExporter are actual
client components. The HTTP preview's one-assignment orchestration is separate
from the production continuous coordinator. No automatic next claim is enabled.
Safe Stop finishes this assignment; cancellation and owner death preserve the
prefix. Conflicting, malformed or incomplete solution records require review.

Solution export succeeds before reporting. The report includes only assignment
and local client IDs; no key, WIF, local path or result file. Failed reporting
leaves the private solution recoverable without rerunning the engine. Terminal
workspaces block repeats and remain available for inspection.

## Validation

The five experimental CTest suites cover protocol, adapter, CLI and HTTP. The HTTP
suite contains 25 scenarios: normal/hash input, completion/progress retries,
lost acknowledgement, pending completion restart, cancellation/owner death,
Safe Stop, prelaunch/during-run rejection, private solution/report recovery,
wrong target/range/identifier, malformed/duplicate/trailing JSON, redirects and
external endpoint refusal.

The parent-death adapter fixture uses an explicit child SIGTERM witness, instead
of looking up a possibly reused or differently namespaced PID in `/proc`.
GPU tests additionally require read-only driver confirmation that the selected
GPU has no remaining compute process.

The standalone kit validates target decoding and CPU reference points without
GPU kernels. `--run` adds 14 real CUDA exact-range tests with first/last/interior
sentinels and out-of-range guards, independent Python HASH160/address/WIF checks,
then the 25 HTTP scenarios with the real engine. These tests use only generated
local targets. They do not benchmark, alter power/fans, install, commit or publish.

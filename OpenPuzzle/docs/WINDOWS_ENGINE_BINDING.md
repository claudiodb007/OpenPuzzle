# Windows native engine binding

Source base: `5526521cce2158651d191cab138aea5725e37f78`.
Engine source/binary foundation: the previously validated standalone v3 port.
This stage adds client-local engine diagnostics; it does not rebuild the engines.

## Commands

The portable layout is `openpuzzle.exe` alongside an `engines` directory holding
`keyhunt.exe`, `clBitCrack.exe` and `cuBitCrack.exe`.

```
openpuzzle.exe selftest --backend cpu
openpuzzle.exe selftest --backend cuda --device 0
openpuzzle.exe selftest --backend opencl --device 0
openpuzzle.exe benchmark --backend cpu --real --seconds 30
openpuzzle.exe benchmark --backend cuda --device 0 --real --seconds 30
openpuzzle.exe benchmark --backend opencl --device 0 --real --seconds 30
```

Benchmark without `--real` only displays the local launch parameters. CPU thread
count defaults to the native logical processor count, capped at 256; `--threads`
allows a smaller count. GPU defaults are blocks 256, threads 256 and points 256;
`--blocks`, `--threads` and `--points` are explicit benchmark options.
Self-tests use fixed one-key parameters, not benchmark tuning.
All arguments are validated before any diagnostic directory is created.

The executable's own machine-readable inventory determines its GPU index. It
must not be confused with the separate driver-enumeration inventory index.
Each self-test checks known scalar 1, known scalar 2 and rejection of a target
outside the inclusive one-key range. BitCrack result addresses and WIF values
are checked against independent known constants. CPU results must retain exact
scalar and terminal coverage.

## Interval and counter contracts

Server assignment bounds remain inclusive. Windows KeyHunt receives `end + 1`
as its exclusive upper bound, including carry and curve-order boundary cases.
The persisted assignment's original end is retained.

The Windows KeyHunt protocol reports private scalars directly, without dividing
counts or speeds by two. Completion requires its exact
`OPENPUZZLE_RANGE_DONE keys_checked=N` marker. Legacy Linux KeyHunt continues to
use the original compressed-hash counter interpretation and `End` marker.

## Process and report ownership

Engine commands use direct CreateProcessW launches via WindowsEngineProcess,
with Unicode argv, a private workspace and an owned Job. No shell is used.
The engine executable's SHA-256 and its device mapping are checked and recorded.
A changed executable during validation prevents success publication.
Timeouts fail self-tests; an owned timeout is expected for the bounded benchmark.
Active or unavailable runtime identities prevent diagnostic competition.

Every explicit run gets a new private directory under
`%LOCALAPPDATA%\OpenPuzzle\data\engine-diagnostics`. Failed runs retain their
logs but do not publish a successful result. Existing results, configurations,
identities and assignment state are not replaced. This stage records benchmark
measurements in diagnostic result JSON; it does not turn them into production
claim profiles.

Benchmarks require 10..60 seconds, at least three post-warm-up samples, and a
reported elapsed duration of at least eight seconds. Samples before five seconds
are excluded. Median speed is reported in MKey/s. A one-second CPU result is not
accepted as a sustained measurement.

## Validation and remaining work

`validate-client-engines.ps1 -EngineDirectory PATH` builds the client with MSVC,
runs 24 native fixture suites, copies the existing three engines into a fresh
portable directory, and runs real synthetic self-tests through the client.
Optional `-Benchmark` additionally exercises sustained real engine benchmarks.
A separate synthetic LOCALAPPDATA isolates validation from existing user data.
The previous engine build and all input files are preserved.

Production assignment launch, solution/recovery integration, application UI,
installer and release publishing remain pending. No production request is made
by this validation or by the local diagnostic commands.

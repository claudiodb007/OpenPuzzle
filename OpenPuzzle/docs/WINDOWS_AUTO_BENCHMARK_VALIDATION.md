# Windows first profiles and automatic benchmark: validation record

This record updates the original preview's pending-hardware notes in
WINDOWS_AUTO_BENCHMARK.md. Evidence types are kept separate below.

## Exact tested implementation

Base: 73fa638be46131941d68cb8e4355556596f2230b.
Application patch SHA-256:
5fff8bb5f3577aa9ff5e77b492319a93da9eb28db020135374e48f8696c80b90.
The integration retains that patch and verifies every client source file
against the validated kit before building and before committing.

The patch covers first-profile creation, safe persistence and reopening,
automatic CUDA/OpenCL parameter selection using the shared Linux matrix,
stability scoring, a 60-second finalist, a 3% planning margin, physical GPU
binding, cancellation and preservation of earlier or damaged profiles.
The CPU route retains an explicit thread count and one measurement.

## Native Windows suite evidence

The user supplied a native MSVC x64 summary: 43/43 suites passed in 64.34 s,
Visual Studio 18 2026, report auto-benchmark-20261019-191618-873bb1ea.
That script verifies registration and JUnit before creating its preview.
The pasted summary does not explicitly identify the Windows version; it is
not recorded as two independent 43-suite runs. Raw Windows JUnit was not
attached for this integration. The second kit revision changed script
progress feedback only; application source stayed identical.

## Manual hardware and interface checks reported by the user

On 2026-10-20 the user reported an overnight Windows 10 CUDA search at
1500.24 MKey/s, 300 W and 60 C in slot cuda-0, after using the automatic
benchmark preview. Total keys were not supplied. These are user observations,
not a measured cross-platform performance comparison.

On the same date the user confirmed Windows 11 operation and then confirmed
all requested checks worked: automatic benchmark, saved profile after
reopening, search and Safe Stop. No additional Windows test log or numerical
benchmark report was supplied with that confirmation.

## Linux regression and commit gate

The integration script must build the complete Linux client and Qt panel,
pass all 176 registered Linux suites, then build and pass all 24 isolated
portable suites. Registration names, JUnit names/counts, failures, errors and
skips are checked before the feature commit. Assertions remain enabled.
The machine-generated results and commit ID are recorded outside the source
worktree in the integration report. This document alone does not assert that
the current Linux integration run has passed.

The original Linux engine binaries are reused and hashed. No installer,
installed profile, live runtime, power/fan setting, release tag or remote
publication is changed. The feature branch starts at the exact preview base;
it does not downgrade the installed stable Linux 1.0.47 release.

## Follow-up scope

The Linux Qt Benchmark button still invokes preview-only CLI behavior in this
base. Connecting it to confirmed automatic measurements, progress and
cancellation is a separate change planned for the next Linux release.
AMD Windows real hardware validation remains outside these confirmations.

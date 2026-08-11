# OpenPuzzle 1.0.10

OpenPuzzle 1.0.10 focuses on a simpler and clearer first-use experience while
preserving the continuous client and assignment protocol from 1.0.9.

## Broader automatic benchmark

The automatic GPU benchmark now evaluates a wider matrix of block and point
values instead of relying on a small set around one estimate. CUDA devices can
also evaluate bounded 512-thread candidates; OpenCL retains the portable
thread matrix. Candidate memory remains capped at 70% of detected VRAM, and
failed candidates cannot replace a valid result.

## Visible private-key recovery

When an engine reports a solution, OpenPuzzle now prints an explicit
`PRIVATE KEY FOUND - ACTION REQUIRED` banner. It exports the wallet import file
to the protected `~/OpenPuzzle-Solutions` tree and creates a visible notice in
the same top-level folder. The notice contains no private key. Wallet files use
owner-only permissions and private keys are never uploaded or printed.

## Actionable diagnostics

First-use, benchmark, engine, configuration and doctor failures now include a
stable `OP-*` error code, a plain-language problem and a concrete action. This
makes common driver, engine, configuration and benchmark failures easier to
identify and report.

## Simple updates

New commands:

    openpuzzle update --check
    openpuzzle update --download-only
    openpuzzle update

The updater obtains the latest release manifest over HTTPS, accepts one
strictly named portable Debian package, verifies its SHA-256 and validates the
package name, version and amd64 architecture before installation. External
commands are executed directly without a shell.

`openpuzzle update` refuses to install while any assignment state or runtime
marker is active. It never stops work automatically; use `openpuzzle safestop`
and rerun the update after all slots finish. Check-only and download-only modes
do not install a package.

Because the update command is introduced by this release, upgrading from
1.0.9 to 1.0.10 still requires the normal one-time Debian package installation.
Later releases can be installed with `openpuzzle update`.

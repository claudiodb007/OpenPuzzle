# Bundled search engines

The OpenPuzzle 1.0 package contains vetted engines for all supported
local execution backends:

- `cuBitCrack` for NVIDIA CUDA GPUs;
- `clBitCrack` for AMD, Intel and other compatible OpenCL GPUs;
- `keyhunt` in range mode for x86-64 CPU execution.

OpenPuzzle does not run arbitrary engine binaries selected from `PATH`
or from user configuration.

## Unified package

The public Debian package is:

```text
OpenPuzzle-1.0.1-Linux-x86_64.deb
```

CUDA is selected when the bundled CUDA engine reports a usable device.
Otherwise, OpenCL can be selected for a compatible GPU. CPU execution
is enabled explicitly and requires a thread count.

## Runtime identity

Before using the bundled executable, openpuzzle runs
the private engine identity operation:

```text
--openpuzzle-engine-version
```

The complete identity must match the protocol and
backend expected by the client. An absent, replaced
or incompatible executable is rejected.

The engine is installed in OpenPuzzle's private
`libexec/OpenPuzzle` directory. Paths previously
stored in `config.json` do not override the bundled
engine.

## OpenCL requirements

The OpenCL package includes the search executable
and depends on the system OpenCL loader. A suitable
vendor runtime must also expose the GPU:

- NVIDIA normally supplies its OpenCL ICD with the
  display or compute driver;
- AMD and Intel can use their vendor runtime or a
  compatible Mesa Rusticl installation.

Device discovery can be checked with:

```text
openpuzzle benchmark --backend opencl
```

The first `openpuzzle run` automatically creates a validated local
benchmark profile before requesting network work. The benchmark may also
be repeated manually:

```text
openpuzzle benchmark --real --auto --backend opencl
```

## Third-party engine sources

OpenPuzzle-BitCrack is derived from the MIT-licensed BitCrack project.
KeyHunt is distributed under its upstream license. Package copies of
these licenses are installed under:

```text
share/doc/OpenPuzzle/third-party/OpenPuzzle-BitCrack
share/doc/OpenPuzzle/third-party/KeyHunt
```

The OpenCL fork corrects high-keyspace progress accounting and requests
the OpenCL 1.2 kernel language standard for compatibility across tested
NVIDIA, AMD discrete and AMD integrated devices. CUDA search code is
kept isolated from these OpenCL changes.

## CPU and concurrent execution

KeyHunt runs in bounded range mode. CPU execution does not use the GPU
benchmark and always requires an explicit number of threads:

```text
openpuzzle run --backend cpu --threads 8
```

GPU and CPU can request and process independent assignments
simultaneously:

```text
openpuzzle run --with-cpu --cpu-threads 8
```

The two execution slots maintain separate assignment state, progress,
checkpoints and lifecycle synchronization. `openpuzzle status` displays
the GPU and CPU slots independently, and `openpuzzle stop` stops both
slots safely.

## Solution result protocol

Protocol 2 writes a structured local result containing the matched address, a
compressed wallet-import-format key and the compression mode. OpenPuzzle
validates this record locally and exports it to a protected file under
`~/OpenPuzzle-Solutions`. Solution contents are never sent to the coordination
service or printed by the client.

# Bundled search engine

OpenPuzzle 1.0 packages include a vetted
OpenPuzzle-BitCrack executable. The client does not
run an arbitrary BitCrack binary selected from PATH
or from the user configuration file.

## Backend-specific packages

OpenPuzzle is distributed as separate packages for
the supported GPU backends:

- OpenPuzzle OpenCL contains `clBitCrack`;
- OpenPuzzle CUDA contains `cuBitCrack`.

A package only enables the backend named in its
filename. Selecting another backend is rejected
before any network assignment is requested.

## Runtime identity

Before using the bundled executable, OpenPuzzle runs
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
OpenPuzzle benchmark --backend opencl
```

Run a real local benchmark before requesting network
work:

```text
OpenPuzzle benchmark --real --auto --backend opencl
```

## OpenPuzzle-BitCrack source

OpenPuzzle-BitCrack is derived from the MIT-licensed
BitCrack project. Its upstream license and the list
of OpenPuzzle-specific changes are installed under:

```text
share/doc/OpenPuzzle/third-party/OpenPuzzle-BitCrack
```

The OpenCL fork corrects high-keyspace progress
accounting and requests the OpenCL 1.2 kernel
language standard for compatibility across tested
NVIDIA, AMD discrete and AMD integrated devices.
CUDA search code is kept isolated from these OpenCL
changes.

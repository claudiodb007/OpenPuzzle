# OpenPuzzle 1.0.9

OpenPuzzle 1.0.9 adds a local backend self-test command for validating an
installed OpenPuzzle client without requesting work from the public service.

## Local backend self-test

Command:

    openpuzzle selftest --backend cuda|opencl|cpu [options]

Available backend-specific options include `--device N`, `--threads N`, and
`--rusticl-enable LIST`.

The self-test:

- runs only the explicitly selected backend;
- validates CUDA and OpenCL with BitCrack and CPU with KeyHunt;
- uses the solved Puzzle 20 test vector over keyspace `80000:FFFFF`;
- performs no assignment, progress, completion, or failure uploads;
- never prints or transmits the built-in private-key test value;
- removes the temporary workspace after success and preserves diagnostics
  when validation fails.

The release also adds CLI regression coverage for the new command and its
argument validation.

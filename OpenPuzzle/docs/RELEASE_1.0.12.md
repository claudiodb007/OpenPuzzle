# OpenPuzzle 1.0.12

OpenPuzzle 1.0.12 introduces safe automatic client updates.

## Safe update

Run:

```bash
openpuzzle update --safe
```

When work is active, OpenPuzzle downloads and verifies the release, requests a
safe stop, allows the current range to finish normally, blocks new assignments
and waits for the runtime to shut down completely. It then installs the package,
checks that the expected version is active and starts `openpuzzle run` again in
the background.

If installation or version validation fails, OpenPuzzle attempts to resume the
client and prints a specific recovery action. The default update mode still
refuses to install while work is active. `--check` and `--download-only` retain
their previous behaviour and cannot be combined with `--safe`.

## Safety guarantees

- Active ranges are never terminated merely to perform an update.
- Package download, checksum and Debian metadata validation happen before the
  running client is stopped.
- Installation starts only after both the active engine and runtime have ended.
- The new client version is checked before the update is reported as successful.
- Automatic resumption writes its output to a private update log.

## Validation

The release was compiled with the portable CUDA, OpenCL and CPU engines and the
complete automated test suite passed before packaging.

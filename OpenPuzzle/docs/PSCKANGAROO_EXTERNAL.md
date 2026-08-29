# External PSCKangaroo executor

OpenPuzzle 1.0.20 does not distribute a PSCKangaroo binary. PSCKangaroo is
GPLv3 software that is built with the NVIDIA CUDA Toolkit, whose runtime has
separate distribution terms. Keeping the executor external avoids combining
those licensing obligations inside the public OpenPuzzle package.

OpenPuzzle discovers an executable named `psckangaroo` in this order:

1. the absolute path in `OPENPUZZLE_KANGAROO_PATH`;
2. `$XDG_DATA_HOME/OpenPuzzle/engines/psckangaroo`;
3. `~/.local/share/OpenPuzzle/engines/psckangaroo` when `XDG_DATA_HOME` is unset;
4. `/usr/local/libexec/OpenPuzzle/psckangaroo`;
5. `psckangaroo` available through `PATH`.

Explicit and local-file candidates must be regular executable files. If none
of those candidates is usable, normal `PATH` discovery is attempted.

The user or system administrator is responsible for obtaining, building and
licensing the external executor. OpenPuzzle must not download or redistribute
the PSCKangaroo binary automatically.

The engine-routing development planner remains fail-closed until the external
executor lifecycle and launch validation phases are complete.


## Local installation command

Run the explicit command below on a Linux x86_64 computer that already has
the NVIDIA CUDA toolkit and driver installed:

```text
openpuzzle engine install psckangaroo
```

OpenPuzzle downloads only the pinned upstream GPLv3 source and builds the
executor locally. The generated executable is stored under the current user's
OpenPuzzle data directory and is not part of the OpenPuzzle package.
Use `--force` to rebuild the same pinned version. The installer does not start
PSCKangaroo or perform GPU work.

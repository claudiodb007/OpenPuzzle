# External PSCKangaroo executor

OpenPuzzle does not distribute a PSCKangaroo binary. PSCKangaroo is
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

## OpenPuzzle 1.0.21 compatibility patch

The installer verifies the pinned upstream Git commit and tree, then applies
the bundled `explicit-seed-v1` patch. The patch adds `-seed <16hex>` for
deterministic distributed walk generations. A missing, modified or
inapplicable patch stops installation before compilation.

After compilation, non-runtime symbols are stripped so CUDA temporary
identifiers do not make otherwise identical builds differ. The user-local
manifest uses schema `openpuzzle-external-engine-v2` and records the upstream
identity, patch and patched-source hashes, generated binary hash, supported
architectures and stripping policy.

The OpenPuzzle package contains the installer and source patch only. The
PSCKangaroo executable is compiled locally after the user explicitly runs:

```text
openpuzzle engine install psckangaroo
```

## Sparse checkpoint storage on Linux (1.0.55)

The same installer now applies the hash-pinned `sparse-checkpoint-v1` patch to
TameStore.h after the existing explicit-seed patch. Both patches and all modified
source files are verified before compilation. The engine manifest retains schema
`openpuzzle-external-engine-v2` and adds checkpoint storage/patch provenance.

The checkpoint writer preserves the exact RCKDT5C-v3 byte stream while seeking
past zero-only table runs. On filesystems supporting sparse regular files, this
reduces allocated disk space for partly empty tables. Logical file size, RAM
capacity, DP selection, collision data and reader behavior are unchanged. Full
tables may offer little or no saving. Windows keeps the dense write path.

Already installed external engines and old checkpoints are preserved. Enable the
new writer only through the normal explicit command, after stopping Kangaroo:

```sh
openpuzzle engine install psckangaroo --force
```

This local rebuild does not launch a search. Checkpoints saved by the previous
engine remain readable, and subsequent saves use sparse storage. The client
update does not compact or delete existing checkpoints. Use `du` to assess
allocated space; `ls -lh` still displays the complete logical size.

## Bitmap checkpoints on Linux (1.0.56)

The bitmap checkpoint patch supersedes sparse writes for newly built engines.
It encodes occupied table slots within each block and selects raw storage when
the bitmap would be larger. The native loader supports both legacy formats.
Explicit resume aborts on a failed checkpoint load.

The installer requires zlib development files (`zlib1g-dev` on Ubuntu). Use the
same explicit local rebuild command above. OPZKCP1 checkpoints need the updated
engine. Keep a legacy checkpoint if a rollback is required.

An installed engine that supports offline conversion can write a compact copy
of a stopped legacy RCKDT5B or RCKDT5C checkpoint.

```sh
"$HOME/.local/share/OpenPuzzle/engines/psckangaroo" --compact-checkpoint \
  /path/to/kangaroo.checkpoint /path/to/kangaroo.compact.checkpoint
```

The converter uses bounded memory, preserves table slot positions and bucket
state, and creates the destination only after writing and syncing the complete
file. It keeps the source intact and requires a new destination filename.
Use a stopped session to keep the source stable during conversion.

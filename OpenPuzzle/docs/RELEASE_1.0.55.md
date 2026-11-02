# OpenPuzzle 1.0.55 — Sparse Kangaroo checkpoint storage

The optional Linux PSCKangaroo installer now applies a second, hash-pinned
source patch. The native writer skips runs of zero-only table blocks using
64-bit seeks. The logical checkpoint byte stream, RCKDT5C version 3 layout,
reader, table capacity, DP policy and search kernels remain unchanged.

Filesystems that support sparse regular files can store unused table regions
without allocating their full logical size. A file may still appear as 31 GiB
in a size listing while occupying less space according to du. Savings depend
on empty regions; dense/full tables are not expected to shrink. The existing
temporary-file, fsync and atomic-rename publication path remains in use.

The previous seed patch is retained. The installer verifies the pinned upstream
commit/tree, both patch hashes, all three modified source paths and the final
patched TameStore hash. The v2 engine manifest retains its previous fields and
adds checkpoint patch/storage provenance. The CUDA compilation flags and target
architectures are unchanged. No PSCKangaroo binary is distributed in the package.

Updating OpenPuzzle does not rebuild or replace an already installed external
engine. To enable sparse writing, finish active Kangaroo work and explicitly run:

```sh
openpuzzle engine install psckangaroo --force
```

This compiles the external engine locally and does not start a GPU search.
Previously saved checkpoints remain readable. They are not compacted or removed
by the update; future saves from the rebuilt engine use sparse writing. The
current memory-planning and checkpoint-resume contracts are preserved.

The added CPU suite exercises the actual pinned checkpoint writer and reader
with controlled table contents: dense/sparse byte identity, legacy-reader resume,
one/two table layouts, full and empty tables, truncated input and injected write,
seek and fsync failures. Five affected CPU/installer suites are run during release
preparation. These tests are not a new real GPU or long-run acceptance result.
No old user checkpoints were found by the earlier audit, so a real-data saving
percentage is not claimed.

Vanity, BitCrack, KeyHunt, common controls, measured GPU profiles, the website API
and scheduling are preserved. The corresponding source archive includes the
pinned GPL checkpoint test fixture and its license. Validation logs, private
wallet files, GPU UUIDs and user checkpoints are not release assets.

# OpenPuzzle 1.0.56

The external PSCKangaroo installer adds compact bitmap checkpoints on Linux.
The new engine reads existing RCKDT5B/RCKDT5C checkpoints and writes OPZKCP1.
Each block retains the original table slot of every occupied entry.
Checksums cover the blocks and checkpoint metadata. A requested resume aborts
when checkpoint loading fails. A failed write preserves the previous checkpoint.

On the RTX 4070 SUPER test machine, a legacy checkpoint resumed, saved in the
compact format, and resumed again in CUDA. It went from 30.87 to 15.64 GiB.
The observed speed in both resume runs was 3.69 GKeys/s. Actual size depends on
the occupancy of the tables.

The engine builds locally through `openpuzzle engine install psckangaroo --force`
with the CUDA toolkit and zlib development files. Stop Kangaroo before changing
its executable. Retain a legacy checkpoint if you may return to an older engine,
since OPZKCP1 requires the new reader.

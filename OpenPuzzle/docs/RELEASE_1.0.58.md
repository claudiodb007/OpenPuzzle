# OpenPuzzle 1.0.58 — Offline Kangaroo checkpoint conversion

The Linux PSCKangaroo installer adds an offline converter for legacy RCKDT5B
and RCKDT5C checkpoints. It writes a separate OPZKCP1 copy with the original
table entries, slot positions and bucket state.

After upgrading OpenPuzzle, rebuild the external engine while Kangaroo is
stopped:

```sh
openpuzzle engine install psckangaroo --force
```

Use the rebuilt engine to convert a legacy checkpoint from a stopped session:

```sh
"$HOME/.local/share/OpenPuzzle/engines/psckangaroo" --compact-checkpoint \
  /path/to/kangaroo.checkpoint /path/to/kangaroo.compact.checkpoint
```

Allow room for the compact copy while converting. The source remains available
for verification or use with a legacy engine. On two real checkpoints, the
combined size fell from 61.75 GiB to 31.56 GiB. The existing OPZKCP1 save and
resume paths also read the converted file.

The Linux desktop status badge now shows when the server has confirmed a
solution for the selected puzzle.

The Linux x86_64 package is `OpenPuzzle-1.0.58-Linux-x86_64.deb`.
The portable updater package uses `OpenPuzzle-1.0.58-portable-XXXXXXXX.deb`,
where `XXXXXXXX` is the first eight characters of the Debian package SHA-256.

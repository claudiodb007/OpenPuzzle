# OpenPuzzle 1.0.54 — Complete Vanity benchmark grid coverage

The automatic Vanity benchmark previously selected five grids around a GPU's
compute-unit count. That omitted two configurations accepted by the existing
native protocol: smaller cards could miss 2048/4096, while larger cards could
miss 64/128. The benchmark now measures the full supported set:
64, 128, 256, 512, 1024, 2048 and 4096 blocks, always with 256 threads and
1024 points. It still validates the best candidate before saving its profile.

Existing measured profiles remain usable. This release does not automatically
replace them or launch a new GPU benchmark during preparation. Future explicit
benchmarks and first-run tuning use seven candidates, so those runs can take
longer. More candidates do not guarantee a speed increase.

The synthetic CLI regression records the native launches and verifies that all
seven grids were tested exactly once, followed by the winning finalist, and
that the selected geometry is saved. Existing checks retain direct parent
ownership, cancellation, failure diagnostics and preservation of saved profiles.
Three affected synthetic/UI suites are run; prior results remain prior evidence.

The native Vanity binary is byte-identical to 1.0.53, including blocking CUDA
synchronization and the already tested multi-architecture kernels. Its five
known-address/WIF checks and short simultaneous rig measurement are retained,
without being described as a new real benchmark or a weekend endurance test.

BitCrack, Kangaroo, KeyHunt, the adapter, one-hour assignment sizing, Stop,
Safe Stop, telemetry, private wallet export, the server API and scheduler are
unchanged. The production website solution notification still has no new
end-to-end acceptance result. Windows remains a separate release.

Finish active work before installation. DEB, byte-identical portable updater
DEB, TGZ, corresponding source and SHA256SUMS are included. Private profiles,
validation logs, GPU UUIDs and test wallet files are not release assets.

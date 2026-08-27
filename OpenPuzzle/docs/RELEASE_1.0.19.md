# OpenPuzzle 1.0.19

OpenPuzzle 1.0.19 makes Rusticl/OpenCL diagnostics consistent with runtime execution and hardens the release update contract.

- A validated `--rusticl-enable <selector>` is persisted as `rusticl_enable`.
- Future OpenCL runs, benchmarks and `openpuzzle doctor` reuse that selector.
- No Rusticl driver is guessed or enabled unless the user selected it.
- `scripts/create_release_manifests.sh` creates both the versioned manifest and the stable `SHA256SUMS.txt` required by the updater.
- The manifest generator validates exactly one portable package and its SHA-256 filename prefix.

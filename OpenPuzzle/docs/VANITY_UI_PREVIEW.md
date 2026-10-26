# Vanity measured-profile Qt preview

Optional Linux-only experimental feature. The Tools button opens a modal Vanity/CUDA panel. Four shared language choices are respected when the panel opens. The installed production engine selection and SQLite profiles remain unchanged.

`vanity-preview measured-import --from FILE --store DIR --enable-experimental-vanity-local` validates the 0.7.0 JSON candidate, exact paired measurements, identities and fingerprints, saves an immutable private candidate and selects it atomically. Repeated identical imports are harmless. Invalid/damaged prior selection blocks replacement. The independent selection record represents import; the candidate itself is unchanged.

`measured-show --store DIR` validates again and emits bounded profile metadata. `measured-test --store DIR --run-root FRESH_DIR --enable-experimental-vanity-local` uses the prior measured CLI/adapter/engine for one 2^36-key absent-target fixture outside Puzzle 71. A fixed numeric 127.0.0.1 listener implements one exact assignment. The new CLI frontend delegates to the source-bound Python helper via execv, retaining process identity for UI cancellation. Python sets a Linux parent-death signal and cleans up only its own child process group. Safe Stop finishes that one assignment; cancellation preserves its checkpoint, with UI resume still pending.

`measured-benchmark --store DIR --enable-experimental-vanity-local` repeats three alternating paired measurements with the validated parameters. It does not auto-tune grid size. Stable complete measurements may select a new candidate; no failed/cancelled profile is selected. UI confirmation is required for each local GPU action.

Full source and reports remain required by hash-bound evidence. This is not a portable installed feature. Test preparation does not run GPU kernels. The real local run requires the bound RTX 4070 SUPER, matching driver/power context and no competing GPU process. No clocks, power or fan writes occur. Production remains disabled.

Validation: native CLI synthetic import/reopen/tamper/preservation tests; actual HTTP/adapter path with fake engine for normal/cancel/Safe Stop; repeated-measurement selection tests; Qt offscreen confirmations, progress, cancellation, close guard, damaged-profile behavior and four languages. A test-harness numeric speed is never a hardware observation.

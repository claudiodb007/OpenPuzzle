# Vanity main controls and local resume — validation record

This feature consolidates the exact Linux preview 0.10.1 source on top of
`c9289b66dc6524ff2953aaf98338846387a746f8`. It follows the existing main UI,
layout, themes and controls: Vanity / CUDA appears in the existing selector.
The measured profile, local progress, saved runs and Resume stay in that window.

## Automated evidence

- Full Linux preview 0.10.0: 187/187 suites, confirmed from its existing JUnit.
- Stop correction preview 0.10.1: 4/4 affected native Qt suites, confirmed from
  its existing JUnit. The full 187-suite run was not repeated for this correction.
- The original held-mouse Stop failure was reproduced during streamed progress.
  The correction applies final button states once and preserves the press until
  release. Regression fixtures cover held Stop, Safe Stop and benchmark clicks,
  four languages, ignored SIGTERM with scoped cancellation, audit, reopen/resume,
  close guard, profile preservation and ordinary main-window controls.
- CLI, measured adapter/engines, prior profiles and recorded evidence are reused
  by content identity. Path copies are not mistaken for binary changes.

## Real GPU confirmation

The user reported completion after resuming from 17,448,304,640 checked keys.
The consolidation verifies the sealed run, archived interrupted checkpoint,
exact contiguous native coverage, two launches, final 68,719,476,736 checked keys,
one original claim, no new resume claim, and one completion. The main UI audit
must also record Stop with the scoped cancellation marker before that resume.
Native owner locks and the profile session lock are checked before consolidation.

The earlier 0.10.0 incident ended completed with safe_stop=0, one launch and no
control markers. Its sealed report and prior validation records are preserved.
The real first resume from preview 0.9.0 remains separately recorded and preserved.
No new GPU test or benchmark is performed while creating this feature commit.

## Scope

This remains a Linux/CUDA local synthetic-assignment preview bound to the prior
RTX 4070 SUPER measurement. Production Vanity assignments are still disabled.
A new automatic Vanity grid tuner, multi-GPU/Windows support, production server
integration, release numbering, packaging and publication are not established by
this commit. It prepares the planned 1.0.49 work without changing release versions.

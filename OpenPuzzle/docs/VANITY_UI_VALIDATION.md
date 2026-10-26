# Experimental Vanity CLI and desktop validation

Consolidated on 2026-10-26 from the Linux 1.0.48 source base `2ad96af0667678dea07679ca9be8806b5ba37df8`.
This is an experimental feature commit, not a release or a production enablement.

## Existing automated evidence

The preserved full Linux JUnit contains 185 distinct passing suites, without
failures, errors or skips. It includes exact-range protocol/adapter, local CLI,
loopback HTTP, measured-profile and Qt panel regressions. Consolidation checks
the exact preview source hashes and executable identities; it does not rerun
tests or measurements. The previous public-target GPU and local HTTP evidence
is checked separately from these synthetic UI tests.

## Real desktop runs confirmed by the operator

On an NVIDIA GeForce RTX 4070 SUPER, the operator exercised the isolated panel.
Each run uses one local synthetic assignment of 68,719,476,736 keys outside Puzzle 71.
The preserved arguments, profile binding, HTTP requests, native checkpoint and
single-launch journal are cross-checked before this document is committed.

| Action | Recorded result | Checked keys | Safe Stop marker |
| --- | --- | ---: | ---: |
| Normal completion | complete | 68,719,476,736 | 0 |
| Safe Stop | complete | 68,719,476,736 | 1 |
| Stop | interrupted / cancelled | 14,763,950,080 | 0 |

Both completed runs acknowledge one completion. Cancellation preserves partial
progress without acknowledging completion. Each recorded run contains exactly
one claim and one engine launch; the panel uses a fresh local run directory.
These are local observations and operator confirmation, not remote attestation.

## Preserved performance evidence

Three alternating pairs compare exact interval size / whole process time:

| Route | Median MKey/s | Dispersion |
| --- | ---: | ---: |
| BitCrack engine | 1493.16 | 0.07% |
| Vanity CLI, loopback HTTP and checkpoints | 2597.33 | 0.07% |

Observed difference: +73.9%. The independent planning rate is
2519.41 MKey/s (3% below the measured median).
The imported profile remains tied to the exact previously measured CLI, adapter,
engine, physical GPU and measurement context. The new desktop/frontend uses
those preserved executables for local tests; rebuilding the frontend does not
assign its identity to the old measurement.

## Remaining limitations

- Production execution, installation, release tags and publication are excluded.
- Resume from the desktop panel remains pending. A saved interrupted checkpoint
  alone does not establish that the panel has resumed it.
- This evidence covers the Linux RTX 4070 SUPER setup, not Windows or other GPUs.
- The experimental CUDA engine remains external and tied to preserved reports;
  this feature commit is not a standalone distributable engine package.
- The benchmark uses the measured fixed geometry; automatic grid tuning remains
  separate work. Python and the source-bound preview helpers are required.
- The known BitCrack unaligned-tail issue remains unresolved. The performance
  comparison uses aligned intervals and the documented bounded target set.

No private GPU identifiers, local absolute paths, raw client logs or profiles
are committed by the validation document. They remain in the local reports.

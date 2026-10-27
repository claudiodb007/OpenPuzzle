# Vanity RunSession and main controls — real local validation

Base commit: `20e3964bae9a33faed76652a2a9ce70db3ea142b`. Local preview: 0.12.0.

The existing main-window Vanity/CUDA controls now route the owned local
coordinator through the public RunSession entry. Ordinary engine delegation,
the measured profile, GPU UUID binding and existing UI layout are preserved.
This is a loopback synthetic Puzzle-20 preview; production Vanity is disabled.

Real GPU validation through the main window:

- Start / Stop: `run-790f9fea-beb8-486c-a41f-00b4d220c437` paused after 9,932,111,872 keys, one claim,
  one engine launch and no completion. The application was closed and reopened.
- Resume: the same run completed 68,719,476,736 keys, three engine launches in total,
  one claim and one completion. The original seal, exact contiguous engine
  batches, archived paused evidence and resumed HTTP suffix were verified.
- Safe Stop: `run-c0fb3dad-e1b4-4fa0-a22a-f1ed35fbd83c` completed 68,719,476,736 keys in two contiguous segments,
  two engine launches, one claim and one completion; no subsequent claim.

Existing validation was reused without rebuilding or rerunning searches:

- Linux: 192/192 suites. The 191 original passed testcase nodes are unchanged;
  VanityCoordinatorDesktopTests was corrected and revalidated separately.
- Isolated platform: 24/24 suites. JUnit failures/errors/skips: zero.
- Original failed JUnit and logs remain preserved under final-batch-recovery.
- Initial JUnit SHA-256: `999ab5f0c98d90e1e874b2030fedddb852e5b2d144294f15f11019dddcf6dd51`.
- Desktop recheck JUnit SHA-256: `36a94b5d578db057e034549def0dc28d94670badd105a1b8ffd141fb14294925`.
- CLI SHA-256: `3076eea50c80f6ac8d7dacf94ab994b7b145d89d30ce6837cf058ab070f024ab`.
- UI SHA-256: `a89eb37e9655c514aa76f751aa5679b8626305b35612df4662b4af283bcceaae`.
- Resumed final seal: `87127333d43e5a3471d49bb94dc13a0035cc76161fe4348f4aa14724f5dd54ff`.
- Safe Stop final seal: `6f2e1b8cb6fc2bd5b8e46f8edb7c5c1597a0c0cbc1ed1d59b60789b889e435ac`.
- Full local evidence: `/home/claudiodb/OpenPuzzle/relatorios/vanity-runsession/session-v80enfqd`.

The final-batch test now distinguishes PROGRESS from a confirmed COMPLETE.
An absent terminal remains review-required; the runtime safety checks were
retained. Confirmed completion can be resumed without another GPU kernel.

No new benchmark, GPU run, installation, production request or publication was
performed during consolidation. Prior commits and reports remain preserved.

Next: extract the loopback assignment contract from the test fixture into a
dedicated local runtime module, preserving this evidence and behavior. Then
develop coordinator pacing, lease/network recovery and explicit production
configuration before enabling production. Windows integration and release
packaging remain separate future work.

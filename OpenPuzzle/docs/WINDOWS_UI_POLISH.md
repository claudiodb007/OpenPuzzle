# Native panel benchmark and scrolling

Safe Stop uses the existing Linux `safe_stop` translation key and still sends
`safestop --slot <selected slot>`. It finishes the current bounded assignment and
blocks another claim; this change does not change search stop semantics.

Benchmark requires an idle measured CPU/CUDA/OpenCL profile. A confirmation
shows the selected physical identity and parameters. It verifies the existing
profile locally before launching the existing native CLI diagnostic for a
30-second measurement. It never requests production work. Stop cancels only the
owned benchmark command; it cannot terminate a search runtime through that API.
The CLI retains its checks for other active/uncertain runtimes.

A successful CPU result is selected from its new private report. A GPU result
first obtains fresh physical UUID/PCI binding through the existing bind-profile
CLI, then becomes selectable. The original profile and engine binary remain
unchanged. GPU binding or measurement failures leave the previous selection
available; incomplete results are not imported. This button repeats the current
profile's parameters; automatic parameter tuning is not implemented.

Layout now batches child repositioning with DeferWindowPos and invalidates the
window once. Parent drawing uses a memory bitmap. Unchanged captions/check marks
are not repainted on every timer tick, and metadata is cached for display rather
than read with ACL operations every 150 ms. Start explicitly rereads puzzle
metadata; Refresh invalidates the display cache. Scrolling at an existing bound
performs no layout work.

The native window fixture switches languages, scrolls repeatedly, runs a fake
benchmark through the actual button/CLI argument route, imports its measured
CPU profile, preserves the original bytes and blocks closing during the command.
Process fixtures verify owned diagnostic cancellation and refusal to terminate a
runtime. Native engine-binding fixtures parse CPU/CUDA/OpenCL UI arguments without
running real hardware benchmarks. Real smoothness, paint artefacts and an optional
user-requested real benchmark require manual Windows review.

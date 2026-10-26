# Vanity main-window stop fix — preview 0.10.1

The 0.10.0 main window applied ordinary-runtime button availability first, then
overrode it for Vanity. During an owned Vanity run this briefly disabled Stop
and Safe Stop on every streamed progress update. Qt clears a pressed button's
down state when it is disabled. An update between mouse press and mouse release
therefore discarded the user's click, leaving no control marker or cancellation.

The issue was reproduced with an actual Qt mouse press held for 180 ms while
the fixture streamed progress every 10 ms. The old implementation failed the
pressed-state assertion. The previous tests used instantaneous `click()` calls
and missed this case.

`MainWindow::updateActionAvailability` now computes the selected engine's final
button state and applies each state once, without the transient disabled state.
Held-click regression checks cover Stop, Safe Stop and benchmark cancellation
in all four languages.

For an active local test/resume, Stop also writes the existing, scoped
`controls/cancel` marker before signalling the owned helper. It validates that
the directory belongs to the selected preview store, refuses symlinks and avoids
writing to a completed sealed run. Benchmark cancellation retains the helper
signal route. No production stop or process-name matching is used.

The preview store gets an owner-only `ui-controls.jsonl` journal with UTC click
time, action, run path, helper PID, last displayed count and whether the cancel
marker was written. Trailing progress no longer replaces the stopping caption.
The window title identifies this fixed build as 0.10.1.

The fixture additionally ignores SIGTERM for a local run and verifies that the
scoped marker still causes cancellation, the timestamped request is recorded,
and the run can be reopened/resumed. Prior panel, benchmark and ordinary UI
control regression checks remain required. Native measured CLI, adapter and GPU
engine binaries are reused unchanged; this is an interface correction.

Reported incident: the 0.10.0 run ended at 68,719,476,736 keys with `complete 0`,
one launch and no control markers. The preparation script verifies and preserves
that sealed report. It builds the UI and four affected Qt suites in a new
worktree, using the existing verified CLI for the subsequent manual GPU review.
The 187 prior Linux suites are checked from their JUnit; they are not repeated
or presented as new full-build results.

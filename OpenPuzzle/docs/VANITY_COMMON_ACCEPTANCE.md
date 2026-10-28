# Complete ordinary main-window acceptance (0.20.1)

The report now includes `Abrir-OpenPuzzle-Vanity.py`. Run it without arguments
for integrity verification only. `--run` opens the existing main window with
Vanity selected, its measured profile, and one bounded local HTTP assignment.
Only clicking Start launches the original measured engine. There is no benchmark,
installation, publication, server modification or production request.

Use the ordinary main controls: Start, nonzero progress, Stop, close the window,
then run the same launcher again and Resume. Safe Stop finishes this assignment
and stops the ordinary runtime. Closing the preview pauses its owned local runtime
and preserves the checkpoint. The original numeric loopback port, client and
assignment IDs and HTTP journal survive reopening. `--new` explicitly selects a
fresh local session while keeping every previous session.

The launcher reads the measured evidence and immutable CLI/adapter/UI/source
bindings. Its home/config/assignments are private to the report. Installed client
state, earlier profiles, worktrees and reports are preserved. If the saved journal
is incomplete, changed or its port is occupied, reopening refuses without launching
a kernel or choosing another assignment.

The peer subclasses the existing LocalAssignmentServer and reuses its private
HTTP journal, locking and transport. It supplies the existing register, heartbeat,
claim, progress and idempotent complete contract. Execution and checkpoint recovery
belong entirely to the ordinary native RunSession/ClientRuntime and adapter.
This is a local acceptance fixture; no PHP/MySQL or deployed API parity is claimed.

A new automated suite connects the actual MainWindow to the complete native CLI
and native adapter, with only engine and GPU inventory synthetic. It exercises
Stop, UI close, peer restart on the same port, UI reopen, Resume, Safe Stop, exact
independent native completion proof, unchanged previous attempts, one claim and
one completion. It also checks journal refusal and the optional launcher without
any automatic GPU execution.

The existing thirteen suites remain valid because native/runtime/UI code and their
original test sources are unchanged by this finishing update. When a compatible
0.20 prepared report exists, the preparer verifies every source/binary/JUnit hash,
preserves it, copies its executable bytes, builds only UiVanityNativeDriver and
runs only the added acceptance suite. The new aggregate has fourteen suites and
records the original provenance of the thirteen reused results. Without a compatible
prepared report it builds and validates the fourteen affected suites normally.
Historical Linux 192/192 and isolated platform 24/24 remain separate evidence.

After preparation, the script prints the exact optional UI command and the local
consolidation command. Consolidation does not rebuild or retest. Production remains
disabled. Kangaroo checkpoint storage/retention follows this Vanity integration.

Use the preparer with `--finish` to validate and consolidate in one invocation.
This creates the local commit, receipt and continuation ZIP after the checks pass,
without an additional confirmation, rebuild, GPU execution or publication.

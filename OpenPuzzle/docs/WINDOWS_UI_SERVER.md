# Automatic coordinator in the Windows panel

Source base: `3eb164164aede1991ad258b8e7306191684197a6`.
The editable-server settings draft was validated on Windows but was never
integrated into the Linux repository. This patch replaces that draft.

The native production panel always uses `https://claudiodb.com`, matching the
Linux runtime's official default. There is no server field, selector, settings
dialog, environment override or persisted UI preference for changing this URL.
`windowsUiStartCommand` forces the official URL, regardless of the selection
object's server value. The technical CLI and generic command builder retain
explicit URLs for existing tooling and isolated loopback fixtures; that option
is not exposed by the panel.

Old windows-ui.json files are ignored and preserved byte-identically. Opening
the panel remains local: profiles, state, sensors and thermal policy only.
Preflight always uses the local dry-run route and does not contact production.
Start still requires a measured profile, confirmed free slot, local puzzle
metadata and operator confirmation. Existing assignment recovery retains its
persisted server; the new-run policy cannot redirect preserved work.

WindowsUiServerTests checks the fixed production argv, hostile environment and
legacy preferences, no client/runtime creation, and local-only preflight. The
window fixture checks absence of the server control and ignores malformed legacy
preferences while retaining theme/scroll/readings/close protection. Real-engine
regression uses explicit isolated loopback assignments, not production claims.

38 Windows suites and 18 isolated portable suites are registered. Full Linux
integration requires 170 tests and the 18 isolated tests before one feature
commit. Native Windows execution and visual review remain separate proofs.

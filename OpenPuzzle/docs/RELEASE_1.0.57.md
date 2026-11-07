# OpenPuzzle 1.0.57 — Kangaroo pause and resume

The Linux main controls add Pause for an active Kangaroo assignment. Pause
signals only the identified native engine and waits for its checkpoint save
acknowledgement. The shell supervisor remains alive while saving. This path
does not apply the ordinary Stop timeout or send a forced kill.

A private assignment-bound intent records the pause before stopping. The
client preserves its execution state and checkpoint, shows Paused, and changes
Start to Resume. Opening the application again retains this choice. The CLI
equivalent is `openpuzzle pause`, then the usual `openpuzzle run <puzzle>`.

Resume uses the existing Kangaroo checkpoint recovery and walk-generation
policy. It verifies the server lease before restarting the same assignment.
A refused/expired lease preserves local data and blocks replacement claims.
Pause does not reserve a server assignment indefinitely. Use Stop to cancel
paused work before starting a new assignment. A cancellation awaiting upload
must be resolved by retrying Stop before Resume is allowed.

The existing Stop and Safe Stop controls remain distinct from Pause. A solution
detected while saving still goes through the existing private export and
metadata-only notification path.

Automated tests cover durable pause intent, slow cooperative saves, failed
saves, checkpoint identity, exit marker preservation, common runtime stop
behavior, lease rejection, pending cancellation, and the Linux controls.
On an RTX 4070 SUPER, a synthetic assignment verified that the CLI and the
Linux controls save an OPZKCP1 checkpoint, reload it, and resume CUDA work
on the same assignment. Reopening the interface restored the Resume action
and selected puzzle. The test used a local server and confirmed that the
client made a single assignment claim.

The Linux x86_64 package is `OpenPuzzle-1.0.57-Linux-x86_64.deb`.
The portable updater uses `OpenPuzzle-1.0.57-portable-XXXXXXXX.deb`, with
`XXXXXXXX` set to the first eight characters of the package SHA-256.

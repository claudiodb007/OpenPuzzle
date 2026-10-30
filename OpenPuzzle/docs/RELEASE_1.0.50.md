# OpenPuzzle 1.0.50

Two corrections to the existing Vanity integration:

- Stop now cancels the current assignment through the ordinary range completion
  API, like BitCrack. The server can recycle cancelled work. Safe Stop still
  finishes the current assignment and blocks the next claim.
- The ordinary heartbeat includes Vanity / CUDA and its detected installation
  state. The existing private Nodes page displays it without a server/schema change.

An explicit cancellation is saved before engine shutdown. Failed or lost HTTP
responses preserve this request for an idempotent cancellation retry, without
renewing the lease, claiming work or relaunching the cancelled range. An engine
interruption without a user Stop retains the existing verified recovery path.
Found solutions retain priority and private export; they are not cancelled.

GPU benchmarks, geometry, profiles, assignment sizing, engines, desktop layout
and server scheduling are unchanged. This Linux update does not replace Windows.
Finish active work and close the desktop panel before updating the installed client.

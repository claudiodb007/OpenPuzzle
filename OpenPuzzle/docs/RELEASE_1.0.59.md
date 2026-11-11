# OpenPuzzle 1.0.59 — Vanity recovery on Linux

For stopped Vanity slots, `openpuzzle status` shows the recorded stop reason,
the time from the exit record and whether the native checkpoint verifies locally.
A missing exit record leaves the time unknown.

The Linux desktop keeps stopped Vanity slots visible with their stop details and
checkpoint status. Slots use two columns to keep multiple GPUs readable.
Each verified slot offers Recover. Confirmation checks the
assignment shown in the card before starting its continuous runtime. If that
assignment changed or disappeared locally, refresh status before retrying.

Use the same device selection as before. For example, to recover a four-GPU rig:

```sh
openpuzzle run 71 --engine vanity --backend cuda --devices 0,1,2,3 --recover-expired
```

OpenPuzzle resumes the assignment from the verified checkpoint when the server
renews its lease. If the server reports `assignment_lease_expired` or
`assignment_not_found`, the flag archives the slot state as
`vanity-retired-client.state` in its assignment directory and requests new work.
That directory keeps its checkpoint and logs. Temporary server failures and
evidence that needs review keep the slot state in place.

Before a resumed adapter starts, the previous progress log is moved to a separate
archive. The monitor then waits for the resumed log to prevent an older sample
from being uploaded after the server has accepted the checkpoint count.

The Linux x86_64 package is `OpenPuzzle-1.0.59-Linux-x86_64.deb`.
The portable updater package uses `OpenPuzzle-1.0.59-portable-XXXXXXXX.deb`,
where `XXXXXXXX` is the first eight characters of the Debian package SHA-256.

# OpenPuzzle 1.0.33 — Multi-GPU recovery and thermal-history safety

OpenPuzzle 1.0.33 improves the behavior of supervised GPU execution when one
worker exits. It also protects the local desktop thermal history if its file
cannot be read.

## GPU telemetry ownership

During a supervised multi-CUDA or multi-OpenCL execution, only one worker
collects the machine-wide GPU sensor snapshot for client heartbeats. When that
worker exits, the supervisor promotes one surviving worker after reaping the
previous owner. The surviving worker keeps its own assignment and begins
including current sensor readings in subsequent heartbeats.

A worker exit does not trigger a GPU reset or restart another worker. The
existing server assignment protocol and private Admin Nodes telemetry format
remain compatible. The public website and network-status API receive no
physical GPU identifiers or temperature and power readings.

## Local thermal-history protection

The Qt desktop interface no longer replaces an existing thermal-history file
with an empty history when that file cannot be read. It reports the problem
and retains the file for diagnosis. A valid history continues to keep the
newest 200 thermal transitions locally, with the existing owner-only access
and confirmed clear action.

## Validation

The complete client suite contains 137 automated tests. It covers shared
telemetry ownership across forked workers, client heartbeat selection,
unreadable thermal-history preservation and the existing runtime, UI, privacy
and packaging contracts.

A live two-GPU validation on a five-RTX-3080 rig used an isolated candidate
from commit `c76a018`. A signal to the `cuda-3` worker produced an orderly
cancellation and final progress upload. The `cuda-4` worker continued its
assignment. More than two minutes later, all five private GPU sensor rows for
the rig had readings updated three seconds before the database query. The
candidate reported version 1.0.32 because the 1.0.33 release identity had not
yet been applied during that test.

## Upgrade

Allow active assignments to finish before replacing the installed package:

    openpuzzle safestop

Then install the new Debian package and verify the runtime:

    sudo apt install ./OpenPuzzle-1.0.33-Linux-x86_64.deb
    openpuzzle --version
    openpuzzle doctor --offline
    openpuzzle status

After a rig reboot, confirm every GPU and reapply any power limits that the
driver reset before starting a multi-GPU run. Thermal policy settings and the
existing client identity remain unchanged by this release.

The portable updater package retains the established identity:

    OpenPuzzle-1.0.33-portable-XXXXXXXX.deb

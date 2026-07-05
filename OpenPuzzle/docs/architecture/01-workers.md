# Workers

A Worker is one execution-capable unit.

Usually this means one GPU, but the model should also support CPUs, remote machines or future accelerators.

## States

idle, running, draining, disabled, offline, failed

## Maintenance

draining means the worker finishes the current job but receives no new jobs.
disabled means the worker receives no jobs.
offline means no heartbeat was received within the timeout.

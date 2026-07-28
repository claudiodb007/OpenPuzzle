# OpenPuzzle systemd user service

OpenPuzzle 1.0.3 includes a systemd user-service template for running
one continuous client without keeping a terminal open.

## Choose one backend

Enable exactly one backend for each Linux user:

```bash
systemctl --user enable --now openpuzzle@cuda.service
```

or:

```bash
systemctl --user enable --now openpuzzle@opencl.service
```

or:

```bash
systemctl --user enable --now openpuzzle@cpu.service
```

Do not enable multiple `openpuzzle@` instances for the same user. The
1.0.3 service template owns the primary execution slot and supports
one selected backend per user.

## Existing manual runtime

Do not start the service while `openpuzzle run` is already active.
Allow the current assignment to finish or stop it gracefully first:

```bash
openpuzzle stop
```

Confirm that the runtime is idle before enabling the service:

```bash
openpuzzle status
```

## Service status and logs

```bash
systemctl --user status openpuzzle@cuda.service
journalctl --user -u openpuzzle@cuda.service
```

Replace `cuda` with `opencl` or `cpu` as appropriate.

## Stop and disable

```bash
systemctl --user disable --now openpuzzle@cuda.service
```

OpenPuzzle receives a graceful stop request. systemd waits up to three
minutes for assignment cancellation, engine shutdown and local-state
cleanup before using its final process-group fallback.

## Start automatically before login

A user service normally starts when the user service manager starts.
On a headless machine, an administrator may explicitly enable lingering:

```bash
sudo loginctl enable-linger "$USER"
```

This is an administrative choice and is never enabled automatically by
the OpenPuzzle package.

## Security and recovery

The service uses a private umask, writes runtime state only under the

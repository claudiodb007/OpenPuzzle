# OpenPuzzle desktop interface

`openpuzzle-ui` is an optional Qt 6 desktop controller for the existing
OpenPuzzle command-line client. It does not replace the CLI, duplicate runtime
state or communicate with the coordination API directly.

The interface launches `openpuzzle` without a shell, refreshes
`openpuzzle status` every three seconds and exposes the existing `safestop` and
`stop` controls. Closing the interface does not stop an active execution.

Executions started from the interface are detached with standard input closed
and both output channels redirected to the private
`~/.local/share/OpenPuzzle/ui-runtime.log` file. This prevents a long-running
client from retaining or filling a graphical-session output socket. The newest
128 KiB remain visible in the Runtime log tab, including the reason for a
temporary wait between assignments. A new manual launch truncates the previous
log and the file is restricted to the current user.

The dashboard keeps a centered maximum content width on large displays and
shows puzzle, engine, backend, speed and synchronization progress as separate
metrics. The raw CLI response remains available in the details panel.

The official OpenPuzzle logo and icon are embedded in the desktop executable.
Users can choose a light interface or the black-and-gold OpenPuzzle theme.
Theme and language preferences are stored locally. English is the default;
Portuguese, French and Spanish are also available from the header.

Common secondary operations are exposed as buttons: benchmark, self-test,
doctor, update check, audit and Kangaroo installation. The interface invokes
the same audited CLI entry points and disables disruptive tools while an
execution is active.

The new-execution card presents five safe modes instead of separate engine and
backend controls:

- BitCrack with CUDA;
- BitCrack with OpenCL;
- BitCrack with concurrent CUDA and OpenCL slots;
- KeyHunt with CPU;
- Kangaroo with CUDA.

Only controls required by the selected mode are shown. OpenCL modes provide an
`AMD GPU via Rusticl (radeonsi)` checkbox, concurrent mode exposes separate
CUDA and OpenCL device numbers, and KeyHunt exposes the required CPU thread
count. The interface never constructs a command through a shell.

An optional checkbox installs and enables a private systemd user service for
the selected search. It is disabled by default and records the selected mode,
puzzle, device numbers, CPU thread count and Rusticl preference. Changing the
checkbox never starts or stops the current execution; it controls only future
system startups. The service uses the same command builder as the Start button
and never invokes a shell.

For execution before graphical login, the administrator must explicitly enable
user lingering once with `sudo loginctl enable-linger "$USER"`. OpenPuzzle does
not request administrative privileges or change lingering itself. Recovery
after a complete power failure also requires firmware configured to power the
computer back on when AC power returns.

## Build

The normal client build does not require Qt. When Qt 6 Widgets development
files are available, CMake builds the interface automatically:

```text
cmake -S . -B build
cmake --build build
```

To disable it explicitly:

```text
cmake -S . -B build -DOPENPUZZLE_BUILD_UI=OFF
```

If `OPENPUZZLE_BUILD_UI=ON` and Qt 6 Widgets is not available, CMake reports
that the optional interface was skipped and continues building the CLI.

The `OPENPUZZLE_CLI` environment variable can point the interface at a specific
client executable during development. Production normally resolves
`openpuzzle` from `PATH`.

## Desktop installation

The interface has its own CMake installation component. This permits installing
or updating only the desktop controller, menu entry and icons without replacing
the command-line client, bundled engines or production runtime state:

```text
cmake --install build-ui --prefix "$HOME/.local" --component DesktopUi
```

The component installs `openpuzzle-ui` under `bin`, a localized desktop entry
and the official black-and-gold PNG icon with rounded corners and a transparent
outer margin in the standard user icon theme. The application identifies
itself as `openpuzzle-ui` to the Linux desktop so the menu entry, running window
and icon remain associated.

The status dashboard creates one card per runtime slot, including CUDA,
OpenCL, CPU/KeyHunt and compatible primary or GPU slots. Command messages and
the automatically refreshed raw status use separate tabs. Automatic refreshes
preserve the detailed-status scroll position and never replace command output.

The header indicator exposes only two stable states: `Running` with a green
background while at least one runtime is active (including the short
`waiting` interval between assignments), and `Stopped` with a red background
when no runtime remains. Polling never replaces the indicator with a temporary
checking state.

When the client reports a potential solution, the interface opens the Messages
tab and displays a translated notice that the protected result was saved below
`~/OpenPuzzle-Solutions`. The notice never reads, displays or uploads the
private key, and repeated status polling does not repeat the same notification.

## Engine rules

- BitCrack supports CUDA or OpenCL.
- KeyHunt uses CPU.
- Kangaroo uses a single exclusive CUDA slot and the interface restricts its
  puzzle selector to 140, 145, 150, 155 and 160.

Safe Stop is disabled while Kangaroo is active because that workload has no
bounded range completion point. `Parar agora` invokes the existing controlled
stop path and preserves any checkpoint produced by the engine.

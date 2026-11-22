# Shared Qt desktop, 2026-11-14

Linux and Windows compile `cmake/QtDesktop.cmake`: the same MainWindow,
GpuControlPanel, thermal controls/history, QRC images, bundled DejaVu fonts,
stylesheet and four translations. No independent Win32 redraw is used by
`openpuzzle-ui.exe`. The old panel remains a test/reference executable named
`openpuzzle-ui-legacy.exe`; it is excluded from the new candidate.

Windows uses the native `openpuzzle-desktop.exe` command adapter. It does not
invoke a Linux shell, WSL, systemd or the Linux runtime. Native engine discovery,
physical GPU binding, measured profiles, job supervision, cooperative controls
and the protected state store remain in the Windows implementation. Devices
are enumerated by capabilities, including OpenCL engine indices different from
CUDA/NVML ordinals, without an RTX/Radeon model allowlist.

The adapter validates automatic GPU benchmarks, binds the native physical
identity, publishes immutable private copies, and updates a per-slot selection
pointer. CPU records match the chosen thread count. Malformed selections,
unknown runtime identity, preserved assignments, missing measurements and
found solutions block a new desktop start. Existing history is preserved.
Runtime speed comes from the verified live process, not the benchmark estimate.
The default puzzle is 0 (the server chooses the first unsolved puzzle).

A dedicated, argument-validated GPU helper requests UAC for power/fan writes;
read-only refreshes do not request elevation. The panel keeps writes busy until
the helper completes. Logs and profile selections use native private-file
protection. No credentials, puzzle claims, keys or remote requests are needed
by the synthetic panel tests.

Unavailable Windows features remain visibly disabled: Kangaroo/Vanity engines,
mixed CUDA+OpenCL runs, systemd autostart, Rusticl environment setup, Linux
updates/audits/install helpers. OS window borders, DPI scaling and font
rasterization may vary; the application widgets/resources are shared.

## Build and validation

Use Windows x64, CMake, a C++20 compiler, matching Qt 6.8+ Widgets, and Boost
headers. `OPENPUZZLE_BUILD_PLATFORM_TESTS_ONLY=ON` builds the Windows native
components; `OPENPUZZLE_BUILD_QT_UI=ON` is the default for this candidate and
requires Qt. With the MSVC Qt SDK, pass `-QtPrefixPath` to
`platform/validate-windows.ps1`. The script deploys matching Qt DLLs/plugins
using windeployqt and requires all 46 registered suites, with no skips.

`WindowsDesktopProtocolTests` rejects malformed/uncertain status.
`WindowsDesktopApplicationTests` covers measured GPU/CPU profiles, physical
binding handoff, official coordinator selection, per-slot dispatch and preserved
history/failures without contacting a server. `SharedDesktopUiTests` drives the
actual MainWindow on Windows and Linux with an isolated executable fixture:
CUDA/AMD selection, benchmark, CPU thread changes, recovery, failed status and
all languages. It can export a screenshot/control manifest with
`OPENPUZZLE_QT_TEST_ARTIFACTS`.

Cross compilation/Wine is preliminary validation. Native Windows acceptance on
NVIDIA and AMD, including long runs and Safe Stop, is required before publishing
or claiming general GPU compatibility. No commit/release/site update is implied
by creating this candidate.


## 1.0.59 candidate revision: RX 5700 XT

The owner confirmed on native Windows that driver enumeration, BitCrack engine
mapping and the desktop-capabilities response all identify `gfx1010:xnack-` as
AMD Radeon RX 5700 XT (8176 MiB, native/engine index 0). The OpenCL benchmark
completed and a real search reported heartbeat/progress at 564.22 MKey/s on
range 11095. This validates that compute path on this machine; it does not
validate the newly enabled ADLX tuning module or all Windows/GPU combinations.
The overnight run remains untouched.

The apparent missing GPU was in the power/fan section: the earlier candidate
had been built without OPENPUZZLE_ADLX_INCLUDE_DIR. The new candidate includes
ADLX control support, displays the detected search GPU even if driver controls
are unavailable, and labels AMD percent power distinctly. A separately
reproduced stale-CUDA preference now selects an available OpenCL GPU on an
AMD-only machine; an explicit CPU selection is retained. Detection errors are
visible and retriable. No driver settings are changed by listing/refreshing.
Windows CLI, panel and HTTP User-Agent identify the same 1.0.59 development build.

# Windows NVIDIA telemetry and thermal controls

Source base: `2df0fdeaf50c41ae6c9f35c252796103f02eb100`.

The Windows CLI now reads NVIDIA temperature, power draw and power limit using
NVML supplied by the display driver. `telemetry` and `doctor --offline` display
these readings without creating configuration or a client identity. Existing
Windows run sessions send valid readings in the optional heartbeat telemetry
payload already supported by the private server dashboard. No server migration
or website change is required. The public API does not receive private keys.

Only the absolute Windows System32 `nvml.dll` is loaded, with a restricted DLL
search policy. This covers the NVIDIA DCH driver layout. Legacy standard-driver
NVSMI paths and AMD telemetry are not implemented in this stage. Missing NVML,
unsupported fields and inconsistent inventories are reported explicitly.
Individual unsupported readings remain absent rather than becoming zero.
An incomplete inventory omits the telemetry update; a complete empty inventory
can clear obsolete readings on the server. Telemetry is sent during a running
client session, not by a background service started by this package.

CUDA indexes are resolved using a normalized PCI bus address, not the NVML
enumeration order. If no unique CUDA match exists, a NVIDIA telemetry ID and
the physical PCI address are retained. OpenCL indexes are not assumed to match
CUDA or NVML indexes. Thermal enforcement requires exactly one NVML GPU with
the name in the selected, previously measured engine profile. Identical GPU
names are deliberately ambiguous at this stage; stable multi-GPU profile
binding is still pending.

## Commands

```powershell
& .\openpuzzle.exe telemetry
& .\openpuzzle.exe thermal
& .\openpuzzle.exe thermal --enable --warning 75 --critical 85 --stop-on-critical
& .\openpuzzle.exe thermal --disable
```

Protection is disabled by default. Explicit policy updates parse all arguments
before writing, lock and reload the current configuration, and publish it
atomically. Unreadable configuration is preserved. Policy changes apply to
subsequent GPU runs; existing sessions keep their policy snapshot. CPU/KeyHunt
runs are not gated by the GPU thermal policy.

When protection and critical stopping are enabled, a critical temperature,
missing selected-GPU temperature or ambiguous GPU identity blocks startup
before requesting an assignment. The temperature is sampled at most once per
five seconds; warnings have a two-degree hysteresis. During execution the same
condition requests the existing orderly stop of the owned engine, preserving
the completion/cancellation protocol. The session returns exit code 2 for a
thermal stop after otherwise successful cleanup. A thermal stop never declares
the unfinished range completed, and never stops an unrelated process.

Warning-only mode (`--no-stop-on-critical`) reports conditions without stopping.
There is no automatic fan, clock, driver, power-limit or voltage setting change.

## Validation

The portable suite tests PCI normalization, reordered CUDA/NVML indexes,
ambiguous mapping, sample caching, thresholds, missing sensors and heartbeat
semantics. Windows fixtures exercise NVML ABI queries, invalid inventory,
read-only CLI commands, concurrent configuration updates, startup gating,
heartbeat readings and cancellation of only the owned process.

`validate-windows-telemetry.ps1 -PriorReport <validated-run-report>` creates a
new native MSVC build, requires all 32 registered suites and matching JUnit,
reads the real NVIDIA sensors and reruns the existing bounded synthetic
assignment proofs with the unchanged KeyHunt/CUDA/OpenCL engines and measured
profiles. It does not rerun benchmarks or request production assignments.
Native Windows execution and actual driver readings must be established on
the Windows PC; a Linux cross-build is not that evidence.

Primary API references:

- https://docs.nvidia.com/deploy/nvml-api/group__nvmlDeviceQueries.html
- https://docs.nvidia.com/deploy/nvml-api/structnvmlPciInfo__t.html
- https://docs.nvidia.com/cuda/cuda-driver-api/group__CUDA__DEVICE.html

GUI, installer, AMD telemetry, stable identical-GPU profile binding and a
controlled production validation remain pending.

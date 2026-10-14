# Windows physical GPU profile binding

Source base: `b3a5da702f3d186dd6b890de1567ab3186fa53d8`.
The preceding telemetry stage passed 168 Linux tests, 16 portable suites and
32 native MSVC suites in the user reports, with real RTX 3080 readings and
synthetic assignments using all three engines. Those results establish the
base, not this new stage's native execution.

## Association and preserved measurements

`bind-profile --profile ORIGINAL --output NEW_FILE` creates a separate copy of
a sustained benchmark profile. Every measured field is retained: executable
fingerprint, device index, name/native/platform/memory mapping, parameters,
speed, sample count and duration. CPU profiles need no GPU association.
The input's bytes are preserved. Existing outputs, already-bound inputs,
invalid paths and unsupported identities are refused rather than overwritten.

The engine's protocol and device JSON establish the selected engine index's
native/platform mapping. A driver must uniquely confirm the name, memory and
mapping and supply a UUID and/or PCI address. NVML must then confirm that same
physical identity and name. The protocol, mapping, inventory and executable
fingerprint are rechecked before returning the association.

An old benchmark did not record physical identity. Migration associates its
measured slot with the physical card currently proven at that slot; it cannot
prove which card was present during an earlier benchmark. It does not invent
a new performance measurement.

Optional `gpu_uuid` and `pci_bus_id` fields are validated by the version-1
profile parser. Bound profiles are checked again before any production claim,
even if thermal protection is disabled. Changed identity refuses the run with
no ordinal fallback. This stage does not automatically remap or overwrite a
bound profile after hardware changes. Older previews do not enforce these
fields; use the new CLI with bound profiles.

## Native driver queries and thermal selection

CUDA uses `cuDeviceGetUuid_v2` (or the original export) and the existing PCI
query. OpenCL checks exact extension tokens before querying
`cl_khr_device_uuid` and `cl_khr_pci_bus_info`. NVML optionally reads
`nvmlDeviceGetUUID`. Buffer sizes and scalar sizes are checked. Zero, malformed
and duplicated identities are refused; unsupported optional fields leave
ordinary hardware enumeration available.

Thermal selection uses UUID first and PCI when no UUID is configured. Name
and any configured PCI address remain consistency checks. A supplied UUID is
never replaced by name matching after a mismatch. Identical names and different
driver/NVML ordering are therefore supported without selecting the hottest or
first-listed card. Missing selected temperature retains existing refusal/stop
behavior. Warning-only legacy behavior and CPU exclusion remain unchanged.

OpenCL UUIDs and PCI topology can change after hardware/driver changes.
Association is revalidated, not treated as an immutable hardware serial number.
Missing extensions and disagreement explicitly refuse binding. MIG UUID
spellings and AMD sensors are outside this NVIDIA stage. No driver, fan,
clock or power-limit setting changes; heartbeat wire fields stay unchanged.

## Validation and limits

The build requires 34 Windows suites, 17 portable suites and 169 full Linux
tests. New portable tests cover repeated names, reversed sensor order, a hot
unselected card, selected overheating, swaps, missing/duplicate identities
and unchanged measurements. Windows CLI fixtures use actual fixture engine
executables with injected driver inventories. The runtime fixture separates
two same-name cards and checks selected owned cancellation and a foreign process.

`validate-windows-gpu-binding.ps1 -PriorReport <completed-telemetry-report>`
builds MSVC, runs all 34 suites, reuses the original engine binaries/profiles,
creates GPU copies and verifies dry-run preflight. It repeats all three engines'
known one-key assignments with protection enabled in an isolated fixture only.
It does not run benchmarks or request production assignments.

Preparation executed all 17 portable suites with direct g++ compilation and
Linux execution. MSVC compilation, 34 native suites, real identity queries and
real bound runs remain pending Windows validation. Actual multiple-GPU Windows
hardware must still be tested: fixtures do not prove real six-card execution.
GUI and installer remain pending.

## Primary API references

- https://docs.nvidia.com/cuda/cuda-driver-api/cuda_driver_api/group__CUDA__DEVICE.html
- https://docs.nvidia.com/deploy/nvml-api/latest/api/group__nvmlDeviceQueries.html
- https://registry.khronos.org/OpenCL/specs/unified/refpages/man/html/clGetDeviceInfo.html
- https://registry.khronos.org/OpenCL/specs/unified/refpages/man/html/cl_khr_pci_bus_info.html
- https://raw.githubusercontent.com/KhronosGroup/OpenCL-Headers/main/CL/cl_ext.h

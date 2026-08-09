# OpenPuzzle 1.0.8

OpenPuzzle 1.0.8 adds supported concurrent CUDA and OpenCL execution on
machines with separate compatible GPU devices.

## Concurrent CUDA and OpenCL

Start the normal CUDA slot and one additional OpenCL slot with:

```bash
openpuzzle run --with-opencl --opencl-device 1
```

The CUDA and OpenCL workers use independent execution slots, assignments,
state files and runtime PID files. `openpuzzle status` reports both slots.

Use `--opencl-device` to select the intended OpenCL device explicitly. This
is especially important when the OpenCL platform exposes both NVIDIA and AMD
devices and CUDA is already using the NVIDIA GPU.

Existing single-backend CUDA, OpenCL and CPU operation remains compatible.
The installer does not silently enable or start a service.

## Server compatibility

The range API distinguishes the `cuda` and `opencl` execution slots so one
client may hold both assignments safely. The private server implementation
and configuration are not part of the public client repository.

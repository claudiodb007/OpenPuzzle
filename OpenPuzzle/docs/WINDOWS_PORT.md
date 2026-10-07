# Windows x64 port: process foundation

Source base: Linux release 1.0.46, commit
`129ada590c4e7591f24251aa4c7fc89e7bfb21db`.

This is the first Windows platform component, not a complete Windows client or
an installer. It introduces a native process runner, process identity checks,
an isolated CMake target and two Windows regression suites. The normal Linux
build continues to select PosixProcessRunner; its release version stays 1.0.46.

## Implemented behavior

- Launch commands through the Windows system cmd.exe, with a Unicode command
  line. Commands use Windows shell syntax; Linux single-quote escaping is not
  accepted as a portable command builder.
- Merge standard output and standard error, split CR/LF records, deliver the
  final partial record, and supply EOF on standard input. Output bytes are
  passed through unchanged, rather than assuming the child's code page is UTF-8.
- Put the suspended command into a Job Object before resuming it. On timeout,
  cancellation or a callback exception, terminate the owned process tree.
- Inherit only the explicitly selected pipe and input handles.
- Check termination with a process handle, including when an exit code happens
  to be the numeric STILL_ACTIVE value (259).
- Query a process creation FILETIME, then compare the expected identity and
  terminate using the same opened handle. Refuse an unreadable or mismatched
  identity. Windows FILETIME and Linux start ticks are different identity
  formats; persisted identities must gain a platform discriminator before this
  component is integrated with recovery.

The fixtures exercise silent and continuous output, closed output, descendant
cleanup, callback exceptions, Unicode executable paths with spaces, unrelated
inheritable handles and repeated launches. They use local helper executables;
they do not call the coordination API or run a search engine.

## Native validation on Windows 11 x64

The delivery ZIP also contains three statically linked x64 fixture/test
executables under `bin/windows-x64`. To execute the two suites immediately,
without installing a compiler, run from the extracted package root:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\OpenPuzzle\platform\validate-windows-prebuilt.ps1
```

The script verifies their hashes, records execution logs and XML results, and
limits each test executable to 45 seconds. These binaries were cross-compiled
with MinGW on Linux. Running them successfully validates native execution;
it does not prove that MSVC can compile the source. The separate procedure below
checks the MSVC build and runs the same suites with CTest.

Install Visual Studio or Visual Studio Build Tools with **Desktop development
with C++**, an x64 MSVC toolchain, Windows SDK and **C++ CMake tools for Windows**.
CMake must be at least 3.22 and support the installed Visual Studio version.
The script discovers the installed compiler; no administrator session is needed
to run the tests after these build dependencies are installed.

From the extracted package root, run PowerShell:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\OpenPuzzle\platform\validate-windows.ps1
```

ExecutionPolicy Bypass applies only to this process. The script creates a fresh
report/build directory under `%USERPROFILE%\OpenPuzzle\relatorios\windows-platform`,
compiles x64 with MSVC and checks that JUnit contains both registered test names,
without failures or skipped tests. Send SUMMARY.txt and, if anything fails,
validation.log and ctest-results.xml. Neither the existing Linux configuration
nor a production installation is used by these tests.

Manual equivalent:

```powershell
cmake -S .\OpenPuzzle -B .\build-windows -DOPENPUZZLE_BUILD_PLATFORM_TESTS_ONLY=ON -A x64
cmake --build .\build-windows --config Release
ctest --test-dir .\build-windows -C Release --output-on-failure
```

## Remaining work before a Windows client can run

1. Port storage locations, AtomicFile, WorkspaceSecurity, configuration locking
   and ClientIdentity. These currently depend on POSIX permissions, file locks
   and Linux filesystem conventions.
2. Connect the process abstraction to RunSession, background launch, runtime
   acquisition, ExecutionStopper and checkpoint recovery. Replace `/proc`,
   fork and signal operations and retain unknown-identity handling. Adapt
   command construction and detached/background process ownership separately.
3. Port HTTP helpers and device discovery, including GpuManager, GpuTelemetry
   and DoctorService. Windows AMD discovery cannot use Linux sysfs or Rusticl
   selectors. Validate NVIDIA and AMD device association on actual hardware.
4. Build and validate Windows engine executables, resolve SQLite/Qt dependencies
   and finish the full application CMake selection. Linux bundled executables
   and their existing checksums cannot serve as Windows engines.
5. Validate the full CLI/UI, update and self-test paths; then create and test
   an installer and a portable package on a clean Windows installation.

These are portability tasks. Finding a Linux-specific API does not establish
another Linux defect. Native Windows test execution is required before declaring
this process foundation validated on Windows.

## References

- [CreateProcessW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw)
- [Job Objects](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects)
- [GetProcessTimes](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getprocesstimes)
- [Microsoft C++ Build Tools](https://learn.microsoft.com/en-us/cpp/overview/acquire-msvc)

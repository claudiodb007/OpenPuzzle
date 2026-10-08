# Windows x64 port: process, storage and runtime control foundation

Source base: Linux release 1.0.46, commit
`129ada590c4e7591f24251aa4c7fc89e7bfb21db`.

The process foundation was committed as
`858045b6b7872a0300c280b90afe0ed6a11377d8` after 154 Linux tests and native
MSVC x64 compilation/execution of its two Windows suites in the user VM. This
storage stage was subsequently committed as
`577e489fba52d81770b6ee91b288ead1ca4c5596`, with 155 native Linux tests,
three isolated Linux suites and four native MSVC Windows suites approved.
This runtime-control stage builds on that storage commit. It is not a complete
Windows client or an installer. The isolated target now registers six Windows suites. The normal Linux
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
  formats. The runtime slot marker now uses an explicit Windows discriminator;
  the separate persisted engine-state/recovery schema is still pending.

The fixtures exercise silent and continuous output, closed output, descendant
cleanup, callback exceptions, Unicode executable paths with spaces, unrelated
inheritable handles and repeated launches. They use local helper executables;
they do not call the coordination API or run a search engine.

## Private storage and local client identity

- Windows settings and `client.id` live under `%LOCALAPPDATA%\OpenPuzzle\config`;
  local client states live under `%LOCALAPPDATA%\OpenPuzzle\data`. An absent or
  relative LOCALAPPDATA fails the operation instead of writing in another folder.
  Linux keeps its existing HOME configuration/data locations.
- Filesystem paths use UTF-16 Windows APIs and UTF-8 persisted strings. Unicode
  names and spaces round-trip through configuration, execution files and recovery.
- Windows creates private files/directories with a protected current-user DACL.
  Existing objects must have the current user as owner. File checks and ACL updates
  use an opened handle. Reads refuse non-disk objects, reparse points and hardlinks.
  These tests require an ACL-capable filesystem such as NTFS.
- AtomicFile writes a separate CREATE_NEW temporary file in the destination
  directory, checks every write, flushes and closes it, then publishes it with
  MoveFileExW. Publication failure preserves the previous destination and removes
  the temporary file. This is single-file replacement, not a multi-file transaction
  or a power-loss durability guarantee.
- ClientIdentity uses a stable, non-inheritable lock file and LockFileEx. All
  contenders open the same file without deletion sharing. Only a missing identity
  creates a UUID; unreadable/invalid identities fail and retain their bytes.
- The Windows client state storage does not invent a Linux boot identifier.
  The engine-state/recovery schema remains pending; runtime slot markers use
  the Windows-specific format described below.

PortableStorageTests checks private access, Unicode paths/data, binary bytes,
concurrent atomic writers, sharing failures, invalid settings/identity retention,
threaded identity access and configuration/state/execution recovery round trips.
WindowsStorageIdentityTests additionally checks eight concurrent first starts,
release of an identity lock when its owning fixture process is terminated,
hardlink refusal and refusal of a junction redirect without altering its target.
These helpers run only against newly created temporary directories and owned
fixture processes. They do not read an installed client's configuration.

Path validation rejects existing junctions/reparse points; this stage does not
claim resistance to arbitrary concurrent replacement of ancestor directories by
another process with the same user rights.

## Runtime slot ownership and cooperative control

- A Windows runtime marker stores PID, `windows-filetime-v1`, and process creation
  FILETIME. The bounded parser rejects incomplete, foreign-platform or future-format
  records. Linux boot identifiers/start ticks are never interpreted as FILETIME.
- Slot acquisition, release and request publication hold a stable private per-slot
  LockFileEx lock. Only one cooperating process can acquire a slot. Release requires
  both the caller PID and creation time to match the published identity.
- Status distinguishes Running, Inactive and Unavailable. Access failure alone does
  not prove termination: a complete process snapshot may prove absence; otherwise
  uncertainty is retained. An opened handle confirms exit or identity mismatch.
- Stop and safe-stop requests contain the complete runtime identity. Repeated
  matching requests are idempotent. Foreign or unreadable requests are retained
  and refused; they cannot authorize removal of a current marker.
- Windows requestStop is cooperative: it publishes a request and never forcibly
  terminates the runtime. A Windows runtime loop must poll stopRequested and
  safeStopRequested, finish its work and release ownership. The local fixtures
  implement that loop; the real RunSession/engine loop is not yet wired to it.
- A proven stale marker may be replaced, removing only requests that match its old
  identity. File publication failures never delete the previous marker first.
  Cleanup of multiple request files is not a multi-file transaction.

WindowsRuntimeRecordTests runs on both platforms. WindowsRuntimeControlTests
uses eight actual fixture processes racing for one dynamic slot, foreign release,
unreadable marker ACLs, old request identities, cooperative stop/safe stop and
recovery after termination of an owned fixture. WindowsProcessIdentityTests now
changes only its owned child process DACL to confirm that an unreadable live
identity remains Unavailable. No installed runtime or search engine is touched.

## Native validation on Windows x64

The delivery ZIP also contains seven statically linked x64 fixture/test
executables under `bin/windows-x64`. To execute the six suites immediately,
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
compiles x64 with MSVC and checks that JUnit contains all six registered test names,
without failures or skipped tests. Send SUMMARY.txt and, if anything fails,
validation.log and ctest-results.xml. Neither the existing Linux configuration
nor a production installation is used by these tests.

Manual equivalent:

```powershell
cmake -S .\OpenPuzzle -B .\build-windows -DOPENPUZZLE_BUILD_PLATFORM_TESTS_ONLY=ON -DOPENPUZZLE_PLATFORM_BOOST_INCLUDE_DIR="$PWD\third-party\include" -A x64
cmake --build .\build-windows --config Release
ctest --test-dir .\build-windows -C Release --output-on-failure
```

## Remaining work before a Windows client can run

1. Connect the process abstraction to RunSession, background launch, runtime
   engine launch, ExecutionStopper and checkpoint recovery. The isolated Windows
   slot acquisition/status/control API exists; the real runtime polling and
   persisted engine identity schema remain to be integrated. Replace `/proc`,
   fork and signal operations and retain unknown-identity handling. Adapt
   command construction and detached/background process ownership separately.
2. Port HTTP helpers and device discovery, including GpuManager, GpuTelemetry
   and DoctorService. Windows AMD discovery cannot use Linux sysfs or Rusticl
   selectors. Validate NVIDIA and AMD device association on actual hardware.
3. Build and validate Windows engine executables, resolve SQLite/Qt dependencies
   and finish the full application CMake selection. Linux bundled executables
   and their existing checksums cannot serve as Windows engines.
4. Validate the full CLI/UI, update and self-test paths; then create and test
   an installer and a portable package on a clean Windows installation.

These are portability tasks. Finding a Linux-specific API does not establish
another Linux defect. Native Windows test execution is required before declaring
this process foundation validated on Windows.

## References

- [CreateProcessW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw)
- [Job Objects](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects)
- [GetProcessTimes](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getprocesstimes)
- [Microsoft C++ Build Tools](https://learn.microsoft.com/en-us/cpp/overview/acquire-msvc)

## Validation status of this runtime-control stage

The sources cross-compile to Windows x64: seven executables, six registered suites.
Four isolated Linux suites pass, including the new runtime record parser. The normal
Linux runtime-control implementation is unchanged; main Linux registers 156 tests.
The delivery environment checks the full CLI/UI build and affected storage/UI tests.
Full native Linux regression proof remains the integration gate: `integrate-linux.sh`
requires 156/156 native Linux tests and 4/4 isolated tests with exact JUnit names and
zero skipped tests before making one commit. Local process/GPU/catalogue fixture
limitations from the preceding stage are not reported as native regression passes.

Native Windows execution and MSVC compilation of these six suites are pending until
`validate-windows.ps1` runs in the VM. The preceding four-suite native storage report
is evidence for the baseline only, not for these new runtime-control tests.

Bundled Boost 1.83 headers retain their copyright/license notices in
`third-party/BOOST_COPYRIGHT.txt`; no Boost binary dependency is introduced.

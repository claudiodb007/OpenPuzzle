# Windows x64 port: processes, storage, runtime, engines and HTTP client

Source base: Linux release 1.0.46, commit
`129ada590c4e7591f24251aa4c7fc89e7bfb21db`.

The process foundation was committed as
`858045b6b7872a0300c280b90afe0ed6a11377d8` after 154 Linux tests and native
MSVC x64 compilation/execution of its two Windows suites in the user VM. This
storage stage was subsequently committed as
`577e489fba52d81770b6ee91b288ead1ca4c5596`, with 155 native Linux tests,
three isolated Linux suites and four native MSVC Windows suites approved.
The runtime-control stage was committed as
`1e7677a4b7a0c09bc521a68b233de9528a50f49d`, with 156 native Linux tests,
four isolated Linux suites and six native MSVC Windows suites approved in the
user reports. The engine-lifecycle stage was committed as
`8740366d1b91710e28962a7d2cd2366bfa7a18f2`, with 157 native Linux tests,
five isolated Linux suites and eight native MSVC Windows suites approved.
This HTTP-client stage builds on that exact commit.
It is not a complete Windows client or installer. The isolated target now
registers eleven Windows suites. Linux continues to select PosixProcessRunner;
its release version stays 1.0.46.

## Existing shell process runner

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

## Direct engine launch and persisted lifecycle

`WindowsEngineProcess` is a separate direct executable/argument API. Unlike the
existing general-purpose shell runner, it uses an absolute application path with
CreateProcessW and never invokes cmd.exe. Its argument encoder follows Microsoft
C/C++ runtime rules, including empty arguments, quotes and trailing backslashes.
Shell metacharacters remain literal argument bytes. Invalid UTF-8, embedded NUL
and oversized command lines are rejected before workspace creation. The application
must pass typed arguments; existing Linux shell command strings are not accepted.

The engine starts suspended, joins an owned Job Object, and publishes its complete
PID/creation-FILETIME/generation identity atomically before resuming. A failed
publication cannot run the child or replace the previous identity marker. A stable
workspace lock serializes concurrent starts. Running or unreadable identities refuse
a replacement. Only selected standard handles are inherited, and stdin receives EOF.

Each new generation has its own private `engine.log` and identity-bound `exit.windows`.
The workspace marker `engine.windows.identity` uses `windows-engine-filetime-v1`;
it is separate from the runtime slot marker and the legacy ClientExecutionState
schema. Exit parsing accepts the full unsigned DWORD range, including 259 as a
completed exit code. Malformed or foreign completion records do not become success.
Log flush and completion publication are checked; failed publication retains existing
bytes and can be retried. Previous generation logs/results are preserved.

Owned descendants are stopped when the engine exits or is stopped. Closing the owning
Job Object on an owner crash also terminates its engine tree. Recovery can distinguish
a live process, proven inactive identity and unavailable identity. A crash without a
published completion does not invent an exit code. This API does not adopt an engine
owned by another live supervisor.

`runWindowsEngineInSlot` acquires the selected runtime slot, polls the owned engine
and cooperative stop requests, and releases only its own slot. Stop and timeout end
the owned tree. Safe stop permits the current engine to finish naturally and returns
that intent to the caller, which must avoid launching another engine. The function
returns exit code, stop intent, safe-stop intent and timeout status; it never requests
an assignment or automatically starts a next engine. The Windows slot selector now
reads the current OS environment value rather than a potentially stale CRT getenv
copy. Dynamic slot selection is asserted in the runtime-control regression fixture.

WindowsEngineFormatTests checks literal argument encoding and strict identity/exit
formats on both platforms. WindowsEngineLifecycleTests uses only local helpers to
check Unicode executable paths, arguments and cwd, fast exits, restricted inheritance,
concurrent launches, preserved publication failures, process-tree cleanup, owner crash,
stop/safe-stop intent and timeout. Its fixtures never call a server or search for keys.
The real RunSession and Windows engine binaries still require integration and testing.

## Native validation on Windows x64

The delivery ZIP also contains thirteen statically linked x64 fixture/test
executables under `bin/windows-x64`. To execute the eleven suites immediately,
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
compiles x64 with MSVC and checks that JUnit contains all eleven registered test names,
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
   slot and engine APIs now exist. Connect their polling and generation records
   to the real assignment loop and existing ClientExecutionState/recovery schema. Replace `/proc`,
   fork and signal operations and retain unknown-identity handling. Adapt
   command construction and detached/background process ownership separately.
2. Connect DoctorService and remaining HTTP helpers to the native transport;
   HttpRangeClient is already connected. Port device discovery, including
   GpuManager and GpuTelemetry. Windows AMD discovery cannot use Linux sysfs or Rusticl
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
- [Microsoft C/C++ argument parsing](https://learn.microsoft.com/en-us/cpp/c-language/parsing-c-command-line-arguments)
- [CRT environment copies](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/getenv-wgetenv)
- [Microsoft C++ Build Tools](https://learn.microsoft.com/en-us/cpp/overview/acquire-msvc)

## Native HTTP transport and the existing client

HttpRangeClient now compiles with WinHTTP on Windows and retains the existing
curl runner on Linux. All six existing operations use the native transport:
client registration, heartbeat, range claim, progress, completion and the solution
signal. This compiles the real client code, not a substitute demonstration client.
The separate OpenPuzzleHttpClient target does not make the entire application
buildable on Windows yet.

Requests preserve UTF-8 JSON bytes and use application/json headers. Shared request
serialization now escapes all JSON control characters and uses the classic numeric
locale. Claim/progress reject negative, infinite and NaN speeds before making a
request; each operation clears stale API error codes. Endpoint names, assignment
parsing, no-work responses and API error-code mapping retain their existing contracts.

The Windows URL policy accepts ASCII HTTP/HTTPS URLs with percent-encoded paths,
valid ports and no embedded credentials, fragments or control characters. Bounds
are 16 KiB for URLs, 2 MiB for request bodies and 256 KiB for response bodies.
Binary and chunked bodies are read with byte counts; oversized or truncated bodies
fail with no partial body reported as success. HTTP failures retain complete,
bounded error bodies for the existing client parser.

WinHTTP uses Windows certificate validation and TLS 1.2, with no certificate
bypass. Cookies and automatic authentication are disabled. Redirects are handled
explicitly: up to three same-origin 301/302/307/308 redirects preserve the method
and body, including POST; a different scheme, host or port is refused. There is no
application retry loop. Connect, send and receive operations have timeouts, and
body reads/redirects check the remaining deadline. These per-operation WinHTTP
timeouts are not a guarantee of one strict wall-clock limit across DNS, connection
and send phases. Loopback fixtures bypass proxy settings; other URLs use WinHTTP's
system proxy configuration.

WindowsHttpTransportTests exercises literal JSON and Unicode, GET/POST/HEAD,
binary/chunked bodies, exact/oversized/truncated responses, canonical redirects,
redirect loops, foreign-origin refusal, authentication challenges, stalled reads,
invalid parameters and the six real client endpoints against a loopback server.
Its broken-TLS fixture confirms that HTTPS attempts TLS and fails a broken handshake;
it does not establish acceptance of a trusted certificate or rejection of an actual
self-signed certificate. Trusted HTTPS and proxy integration remain to be validated
before the full client uses a production server. No production API or GPU engine
is used by these fixtures.

## Validation status of the HTTP-client stage

Windows x64 cross-compilation produces thirteen executables and eleven registered
suites. This stage adds HttpRequestPolicyTests, HttpRangeClientTests and
WindowsHttpTransportTests to the eight previously validated Windows suites.
Native execution and MSVC compilation of this eleven-suite package remain pending
until validate-windows.ps1 runs in the VM.

The delivery environment verifies the complete Linux CLI/UI compilation, six
HTTP/client-related suites and six isolated platform suites. It registers 158 main
Linux tests. ExecutionSyncServiceTests also ran locally, but its self-process recovery
fixture failed identically when relinked with the unchanged baseline HttpRangeClient;
that environment result is preserved with the baseline comparison under verification.
It is not counted as a regression pass. Full native regression is the integration
gate: integrate-linux.sh requires 158/158 Linux tests and 6/6 isolated tests, exact
CTest/JUnit names and zero skipped tests before creating one grouped commit.

Bundled Boost 1.83 headers retain their copyright/license notices in
third-party/BOOST_COPYRIGHT.txt; no Boost binary dependency is introduced.

HTTP API references:

- [WinHttpOpen](https://learn.microsoft.com/en-us/windows/win32/api/winhttp/nf-winhttp-winhttpopen)
- [WinHttpSendRequest](https://learn.microsoft.com/en-us/windows/win32/api/winhttp/nf-winhttp-winhttpsendrequest)
- [WinHttpReadData](https://learn.microsoft.com/en-us/windows/win32/api/winhttp/nf-winhttp-winhttpreaddata)
- [WinHttpSetTimeouts](https://learn.microsoft.com/en-us/windows/win32/api/winhttp/nf-winhttp-winhttpsettimeouts)
- [WinHTTP option flags](https://learn.microsoft.com/en-us/windows/win32/winhttp/option-flags)

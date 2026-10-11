# Windows x64 port: assignment state and execution synchronization

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
The HTTP-client stage was committed as
`213ec3119586847f00bb8bd20eedbd99b5e342d2`, with 158 native Linux tests,
six isolated Linux suites and eleven native MSVC Windows suites approved.
The Windows execution-synchronization stage builds on that exact commit.
It is not a complete Windows client or installer. The isolated target now
registers thirteen Windows suites. Linux continues to select PosixProcessRunner;
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
  the assignment state now carries the explicit Windows identity described below.

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
  The assignment state now carries an explicit Windows identity; runtime slot markers
  use the separate ownership format described below.

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
it is separate from the runtime slot marker and the runtime owner record in ClientExecutionState. The assignment state now
references this engine generation explicitly. Exit parsing accepts the full unsigned DWORD range, including 259 as a
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

The delivery ZIP also contains eighteen statically linked x64 fixture/test
executables under `bin/windows-x64`. To execute the sixteen suites immediately,
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
compiles x64 with MSVC and checks that JUnit contains all sixteen registered test names,
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
   to the real assignment loop. Windows assignment state and ExecutionSyncService are
   now connected through startWindowsExecution; RunSession is still pending. Replace `/proc`,
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

## Windows assignment state and synchronization

`startWindowsExecution` launches a single already-assigned job using the existing
ClientExecutionState and ClientStateStore. The caller must own the runtime slot.
A private per-slot publication lock protects both launch and synchronization.
An existing state file, including an invalid/unreadable one, refuses a new launch;
a pending network acknowledgment cannot be overwritten. Metadata is checked before
creating a child. The engine remains suspended until its generation identity and
assignment state have both been published. Failed state publication terminates the
owned suspended child before any engine instruction runs.

Windows state stores process_platform (`windows-engine-filetime-v1`), process
creation FILETIME, process_generation, and the owning supervisor's PID/FILETIME.
The Linux boot ID is empty. Legacy Linux files still load with empty Windows fields;
foreign or incomplete identities cannot authorize Windows progress or completion.
This is single-file publication with an owned suspended child, not a transaction
across all workspace files or a power-loss durability guarantee.

The actual existing ExecutionSyncService now compiles on Windows. It reads only
engine.log and exit.windows from the exact persisted generation. Root legacy logs
and exit.code cannot authorize completion of a new Windows engine. A missing,
unreadable or changed identity/generation preserves the assignment without a request.
When an engine has exited but its supervisor is still flushing/publishing completion,
the service retains uncertainty. If both engine and supervisor are proven inactive
and no exit was published, recovery sends interruption (`cancelled`, -3), never
completed coverage.

Completion still requires both a successful native exit and deterministic engine
proof. BitCrack's end-of-keyspace marker and KeyHunt's exact End record retain their
existing contracts. Kangaroo's probabilistic exit is not complete-range proof.
Successful completion sends the exact inclusive assigned-range count, not the last
periodic sample. Temporary upload failures retain pending state for retry. Acknowledgment
or the existing assignment-rejection contract permits state removal.

The full unsigned Windows exit code remains in the generation record and nativeExitCode.
Values above signed INT_MAX map to the existing API's signed failure sentinel -4;
they never wrap into success. found.txt/results are inspected as paths only: a solution
preserves assignment state and is not read or transmitted by this service. A workspace
with an existing solution refuses a new managed launch.

Optional adaptive profile calibration remains unavailable on Windows and reports that
limitation explicitly; SQLite/profile calibration is not silently substituted. The real
RunSession loop, assignment acquisition, GPU engines, full CLI/UI and installer are
still pending. This API does not automatically claim work or launch a next assignment.

WindowsExecutionStateTests checks explicit identities, legacy compatibility and Unicode
state round trips on both platforms. WindowsExecutionSyncTests uses the real service,
private temporary workspaces, owned helper processes and loopback HTTP. It checks pending
state protection, exact-generation progress, unavailable identities, owner publication
windows, proof, retry/acknowledgment, local solutions, DWORD failures, refused publication
before resume, and abrupt supervisor termination. No production endpoint or GPU engine
is used.

## Approved execution-synchronization baseline

Commit d111a7e0a6f81567a79361a7473d14982648dd95 is the exact baseline for
this stage. The user's reports establish 159/159 native Linux tests, 7/7 isolated
Linux suites, and 13/13 native MSVC x64 suites on Windows 11. These completed
components are retained; the new patch does not reapply their changes.

## Shared Windows continuous loop and assignment monitor

The actual ClientRuntime continuous loop and monitor now compile in the isolated
Windows build. Linux production dependency construction is moved, unchanged, to
ClientRuntimeProduction.cpp. Linux default constructors and thermal observers are
retained. Windows explicitly constructs dependencies through WindowsRuntimeSession;
it cannot silently instantiate Linux hardware/service dependencies.

WindowsRuntimeSession requires a server URL, an idle capability heartbeat and a
mandatory caller preflight. Its preparation callback supplies already-assigned
metadata, an absolute executable and typed arguments. No production claim,
registration, GPU discovery or engine argument planner is added by this adapter.
Capability entries must explicitly mark the engine/backend installed and available;
a GPU launch also requires the corresponding inventory entry. These declarations
are supplied by the caller, not an independent hardware discovery performed here.

The existing runtime loop acquires/releases the identity-bound slot. An initial
idle heartbeat must be acknowledged before the first preparation callback; later
iterations retain the existing availability acknowledgement gate. Any pending file,
including malformed/unreadable state, blocks preparation. The adapter launches via
startWindowsExecution, retains the owned Job handle, polls completion publication
and uses the real ExecutionSyncService and HttpRangeClient. It does not adopt or
terminate another supervisor's engine. Stop and safe-stop markers retain their
identity contracts; safe stop completes the current assignment and blocks another.

Native waits poll the owned child and stop marker. A one-shot wake shortens the
monitor's usual 60-second polling interval when completion, initial progress or a
local result appears. Temporary upload failures keep the normal retry interval
and preserve state. The shared loops' Linux behavior is unchanged when this optional
wake dependency is absent. Cancellation keeps final progress and pending state if
its acknowledgment fails. Console Ctrl-C/Break request cooperative stop; console
close/shutdown termination is not claimed as graceful cancellation.

The production SolutionExporter now uses the existing Windows private-file and
atomic-file helpers. Results are read through bounded protected handles, refusing
hardlinks/reparse paths. Wallets and notices are stored below
%LOCALAPPDATA%\OpenPuzzle\solutions\Puzzle-N\assignment-id, using Unicode paths and
protected current-user DACLs. Stable per-file locks serialize publication; existing
exports are preserved byte for byte. Notices exclude key content. The monitor sends
only the existing solution signal and IDs; result content remains local. Linux
export paths, parser contracts and publication behavior are retained. The existing
WIF parser performs syntax checks, not new cryptographic checksum verification.

PortableSolutionExporterTests checks private exports, bounded input, metadata,
Unicode paths and preservation; Windows additionally checks protected ACLs and
hardlink refusal. ClientRuntimeLoopTests exercises the actual common loops with
controlled dependencies, including safe stop, idle acknowledgement, wake, retry
intervals, failed stop and unknown identity preservation. WindowsRuntimeSessionTests
uses owned helper processes and loopback HTTP to exercise the actual adapter and
loops, preflight/capability refusal, pending-state blocking, natural completion,
safe stop, failed cancellation acknowledgment, unrelated-process preservation, uncertain-identity refusal and
local export with a metadata-only report. Fixtures contain synthetic material only.

## Validation gate for this stage

The package contains 18 x64 fixture/test executables and 16 registered Windows
suites. All 16 still require native Windows 11/MSVC validation for this new delivery.
The Linux integration script requires 161/161 full tests including UI and 9/9
isolated suites, matching exact CTest/JUnit names with no skips, before one commit.
Cross-compilation and Linux delivery results are recorded under verification;
these do not substitute for native MSVC execution.

The real RunSession orchestration (including registration/claim and engine planning),
Windows GPU discovery/telemetry, adaptive calibration, full CLI/UI, production HTTPS
and installer remain pending. This stage does not release a new product version or
change production installation, runtimes, tags or remote publication.

Bundled Boost 1.83 headers retain their copyright/license notices in
third-party/BOOST_COPYRIGHT.txt; no Boost binary dependency is introduced.

HTTP API references:

- [WinHttpOpen](https://learn.microsoft.com/en-us/windows/win32/api/winhttp/nf-winhttp-winhttpopen)
- [WinHttpSendRequest](https://learn.microsoft.com/en-us/windows/win32/api/winhttp/nf-winhttp-winhttpsendrequest)
- [WinHttpReadData](https://learn.microsoft.com/en-us/windows/win32/api/winhttp/nf-winhttp-winhttpreaddata)
- [WinHttpSetTimeouts](https://learn.microsoft.com/en-us/windows/win32/api/winhttp/nf-winhttp-winhttpsettimeouts)
- [WinHTTP option flags](https://learn.microsoft.com/en-us/windows/win32/winhttp/option-flags)

## Runtime-session package v2: process test diagnostics

The user Windows/MSVC run for package v1 passed 15/16 suites, including all three
new runtime-session suites. WindowsProcessRunnerTests failed at its first
"successful child exit" assertion. That assertion did not report whether launch
failed, the exit code, the elapsed time or collected output; the root cause is
therefore not established by that report.

Package v2 changes test/validation code only. Successful fixture launches allow
15 seconds instead of 3, report started/exit/expected/elapsed/output on failure,
and are not retried. Real timeout cases retain their one-second deadline and
six-second upper bound. Cancellation deadlines remain unchanged. The runner suite
has a 90-second outer bound in CTest and prebuilt validation; other suites keep
their existing bounds. Temporary executable copies use distinct names and refuse
to overwrite an existing file. WindowsProcessRunner production code is unchanged.

All 16 suites require a fresh native Windows/MSVC run for v2 before integration.
This revision does not claim that the reported launch failure is fixed or that
Windows antivirus caused it; the additional diagnostics establish the next failure
precisely if the larger normal-start allowance is insufficient.

## Windows run command preparation (base 0cb20ba)

The user completed the runtime-session stage in commit
0cb20ba8019034f66125814af23edab5de455395. Its source bytes match the delivered v2;
the Windows 11/MSVC x64 report passed all sixteen suites. This stage builds on
that commit and does not repeat or change the previous foundation commits.

RunCommandOptions implements a strict, side-effect-free parser for run, direct
puzzle/options, help/version and status/stop/safestop. It preserves puzzle=0 for
server selection of the lowest open puzzle. Explicit puzzle and launch integers
are bounded by int, durations by 1..360; unknown, duplicate and incompatible
options are refused before identity creation or network calls. Concurrent device
lists, claim-only and Linux-only options are deliberately refused in this stage.

RunAssignmentPlan is pure: it validates identifiers, target syntax, inclusive
hexadecimal ranges, puzzle/backend agreement and explicit metadata bounds, then
produces an argv vector and state for linear BitCrack CUDA/OpenCL or KeyHunt CPU.
It never builds a shell command or prepares files. Windows reserved directory
names and unsafe assignment path components are refused. Unicode workspace paths
remain UTF-8 in state and argv; CreateProcessW receives the existing wide encoding.
Address validation is syntax-only, retaining the prior catalog contract.

WindowsRunSession provides the native command orchestration over
WindowsRuntimeSession and the real HttpRangeClient. A platform provider supplies
verified executable bindings, device inventory, heartbeat capabilities, local
metadata and engine/hardware/profile preflight. These declarations do not replace
independent Windows hardware discovery or executable provenance checks. Explicit
metadata/routing and preflight are validated before work. Dry-run/preflight-only
remain local and do not create identity, acquire a runtime or call the server.

An initialization callback inside the acquired runtime registers the stable local
identity before the initial idle heartbeat; both acknowledgments precede claim.
The previous three-argument runtime construction and default continuous behavior
remain compatible. --once terminates after one completed or unavailable request;
registration/heartbeat errors terminate a one-shot run without a claim.

A private intent is persisted before claim. A valid response replaces it with the
assignment receipt before planning or launching. An ambiguous response keeps the
intent and blocks further claims, including after restarting the command. Malformed
assignment metadata, an occupied workspace or failed launch preserves the receipt
and existing bytes. This stage does not automatically reconcile uncertain claims;
remote lease/receipt recovery is still required before requesting new work. The
receipt is discarded only after an acknowledged successful execution and removal
of its engine state. Existing/preserved solution state remains protected.

Typed engine arguments launch through the established Windows engine/session
supervisor, monitor, progress/completion API and private exporter. No shell, POSIX
fork/wait or Linux workspace preparation is used. KeyHunt targets are written
privately; its existing result filename is handled by ExecutionSyncService.
Status is local and distinguishes unavailable identities/unreadable state and a
preserved pending claim; stop and safestop publish identity-bound cooperative
markers. Real engine binaries have not been executed or validated by these tests.

This package registers nineteen native Windows suites (the previous sixteen plus
RunCommandOptionsTests, RunAssignmentPlanTests and WindowsRunSessionTests), with
21 x64 helper/test executables. Native fixtures test registration-before-heartbeat,
failed initialization, automatic puzzle selection, CUDA/CPU direct argv, once,
continuous safe stop, unavailable work and preserved invalid/uncertain claims and
workspaces. Only local synthetic helper processes and loopback HTTP are used.
The two common suites also run on Linux. Native Linux integration requires all
163 full CLI/UI suites and 11 isolated suites with exact JUnit/CTest names before
one commit. Cross-compilation is not native MSVC validation.

Still pending: standalone Windows Application/CLI/UI wiring, native GPU discovery
and telemetry, benchmark/profile provider, Kangaroo typed planning/checkpoint
recovery, ambiguous-claim reconciliation, production HTTPS acceptance and installer.
The Linux RunSession/Application behavior and product release 1.0.46 remain unchanged.

## Native Windows local CLI (base 5472e62)

The run-preparation stage is already committed as
5472e6274f8f89d2fab4475b6fc4867f0cd1a797, with user reports proving
163 full Linux suites, 11 isolated Linux suites and 19 native MSVC x64 suites.
This stage adds the first standalone console entry, not a production search client.

The platform build produces openpuzzle.exe through target OpenPuzzleWindowsCli.
Its wmain converts native wide arguments to UTF-8 with invalid Unicode rejection;
console output uses UTF-8. It is launched directly without a shell. With no
arguments it shows help and never starts automatically. Help/version are available
without LOCALAPPDATA, configuration or a capability provider.

Local commands are help/version, paths, doctor [--offline], status [--all], stop
and safestop. --slot accepts only existing canonical ExecutionSlot names; duplicate
or invalid selection is rejected before changes. A malformed environment slot
is refused rather than silently falling back to primary. Explicit slot selection
is supported; run dispatch restores the OS environment on success or failure.
The dispatch API is serialized and must not be called concurrently because the
established runtime reads the process-wide slot environment.

Status inspects the established identity-bound runtime and synchronization state;
--all also finds valid pending-claim-only slots. Unavailable identities, unreadable
state and pending receipts are retained and produce exit code 2. Stop/safestop
publish cooperative requests only for the selected verified owner; acknowledgment
of a marker does not claim that the engine has stopped. Local controls never
force-terminate a PID or change another slot.

Doctor is offline even without --offline. It reports native x64 logical processor
count, physical memory and local paths, reads existing configuration through the
private checked loader, and distinguishes missing settings from invalid data.
It does not write defaults, create client identity, contact a server, execute any
engine or run a benchmark. Existing file contents remain unchanged; the established
private loader can harden ACLs on files it reads. Successful local diagnostics
are explicitly not reported as GPU readiness or production search readiness.

A fresh Windows profile exposed a prior private-reader edge case: Win32 returns
ERROR_PATH_NOT_FOUND when intermediate directories are absent. After safePath
checks, this is now classified as missing alongside ERROR_FILE_NOT_FOUND. Access
denials/reparse points remain unavailable. Thus a nonexistent profile is idle,
not an uncertain owner. The new native tests cover this before any directories
or runtime records have been created.

Native CLI validation also reproduced profile creation during status inspection:
ClientStateStore::load prepared the data directory before checking for a state
file. Reading now opens and protects only an existing file; directory creation
stays in save. A missing state cannot create profile directories, including if
the parent disappears during the read. PortableStorageTests checks this on both
platforms. The native CLI test retains its no-profile assertion and checks each
information command separately for clearer failure diagnostics. Existing state
contents and private file protection are preserved.

WindowsCliApplication can receive the verified WindowsRunDependencies used by
the preceding stage. The standalone executable deliberately supplies no production
bindings: run is parsed, then refused by the CLI before identity/network/launch
if provider support is absent. With a supplied provider it routes through
WindowsRunSession and its established verification gates. It does
not turn an executable path or a fixture into a trusted search engine.

WindowsCliApplicationTests launches the actual new executable from a Unicode path
containing spaces and ampersand, checks wide argument conversion and exit codes,
uses an isolated LOCALAPPDATA, and exercises fresh profiles, slot restoration,
wrong-slot controls, invalid configuration and preserved pending/unreadable state.
Only synthetic local files and supervised local executable processes are used.
There are now 20 registered native suites and 23 x64 executables: 20 tests, two
helpers and the CLI preview. All 20 require native Windows 11/MSVC validation;
cross-compilation alone is not an execution result. Linux test counts stay
163 full suites and 11 isolated suites; both are required before the single commit.

Still pending: real engine Windows builds/provenance, GPU discovery/telemetry,
benchmark/profile provider, enabled standalone run, Kangaroo and recovery,
production HTTPS acceptance, desktop UI and installer. The full Linux application,
Linux release 1.0.46, installations, live runtimes and release tags are unchanged.

## Native CPU and CUDA/OpenCL discovery (base c6224dc)

The local CLI stage is already committed as
c6224dc51483862482c0561b662c4e6d86bdae32. User reports prove 163 full Linux
suites, 11 isolated Linux suites and 20 native Windows/MSVC x64 suites.
This stage retains those components and adds read-only hardware discovery.

`openpuzzle devices` reports native x64 CPU logical processors and physical
memory, the CPU/KeyHunt engine requirement, and independent CUDA and OpenCL
inventories. It works without LOCALAPPDATA or an execution slot. Extra options,
including --slot, are rejected before driver queries. It creates no profile or
client identity and sends no network request. Doctor also includes this inventory.
KeyHunt remains the CPU engine in RunCommandOptions/WindowsRunSession; CPU
assignments and direct argument routing already have regression fixtures.
The Windows KeyHunt executable, provenance, self-test and benchmark are still
required before the standalone client can search. An OpenCL CPU device is not
treated as a GPU or as proof that KeyHunt is available.

CUDA uses the x64 CUDA Driver API from the absolute Windows system nvcuda.dll:
initialization, driver version, device count, name, memory and multiprocessors.
No CUDA Toolkit, nvidia-smi, context, kernel or search process is required.
OpenCL uses the absolute Windows system OpenCL.dll to enumerate all platforms
and devices, then filters CL_DEVICE_TYPE_GPU. Native platform and device
positions are retained even when an earlier device is a CPU. A separate flattened
inventory index is diagnostic only; it is not asserted to match BitCrack's index.
Memory sizes use the documented 64-bit ABI. Strings, counts, returned sizes,
duplicate handles and driver failures are validated; partial failing backend
inventories are discarded. Loading uses LOAD_LIBRARY_SEARCH_SYSTEM32 with no
current-directory, application-directory or PATH fallback.

Missing driver libraries, no devices and query errors are distinct. Missing CUDA
is expected on an AMD-only PC and does not block the independent CPU/OpenCL
inventory. CUDA errors do not hide OpenCL devices, or vice versa. Exit 0 means
enumeration completed (possibly without a GPU); exit 2 means at least one native
hardware query was unavailable. Neither exit proves an engine, kernel, benchmark
or production search is ready. CL_DEVICE_AVAILABLE/COMPILER_AVAILABLE are driver
properties, not substitute engine tests. Existing runtime/configuration uncertainty
continues to be preserved by doctor. Its local CLI fixture injects a deterministic
inventory; the real executable separately accepts and identifies hardware-query
failure so regression results do not depend on a physical GPU.

WindowsGpuDiscoveryTests covers AMD and future NVIDIA inventories, mixed CPU/GPU
platforms, more than 4 GiB of memory, unavailable devices, API failures, malformed
driver output, bounded allocations, absolute system paths and profile-free CLI
dispatch. The package registers 21 native Windows suites and contains 24 x64
executables (21 tests, two helpers and the CLI). The validators also capture
`hardware-inventory.txt` from the real PC after the fixture tests, recording its
separate diagnostic exit code. A fixture pass is not physical GPU acceptance.
Linux counts stay 163 full and 11 isolated suites. New native Windows/MSVC
execution is pending until the user runs this package.

Remaining work includes native KeyHunt CPU and BitCrack CUDA/OpenCL engine builds,
verified device mapping, kernel tests, benchmark/profile providers, GPU telemetry,
enabled standalone run, UI and packaging. AMD RX 5700 XT is the current test PC;
NVIDIA acceptance will use the user's later NVIDIA PC. No GPU model is hardcoded
in production discovery, and no product version, release tag or install changes.

Primary API references:
- https://docs.nvidia.com/cuda/cuda-driver-api/cuda_driver_api/group__CUDA__DEVICE.html
- https://registry.khronos.org/OpenCL/specs/unified/refpages/man/html/clGetDeviceIDs.html
- https://registry.khronos.org/OpenCL/specs/unified/refpages/man/html/clGetDeviceInfo.html
- https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryexa

## Native client engine binding

The standalone Windows KeyHunt, OpenCL and CUDA engines passed native MSVC
synthetic searches and local benchmarks on the user RTX 3080 system.
The next client binding stage adds explicit local `selftest` and `benchmark`
commands, exact inclusive-to-exclusive CPU range conversion and scalar counter
parsing. See [WINDOWS_ENGINE_BINDING.md](WINDOWS_ENGINE_BINDING.md). Production
assignment/solution/recovery integration and UI remain pending.

## Measured run and recovery stage

The client engine binding base `2df6989` now has an explicit measured-profile
provider and recovery command. See [WINDOWS_NATIVE_RUN.md](WINDOWS_NATIVE_RUN.md)
for exact scope, source/benchmark verification, protected KeyHunt exports,
conservative receipt handling and the loopback-only real-engine validation.
This stage does not establish production operation or a Windows UI/installer.

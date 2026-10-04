#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/runtime/ClientRuntime.hpp"
#include "openpuzzle/runtime/ClientRuntimeControl.hpp"
#include "openpuzzle/runtime/LinuxProcessIdentity.hpp"

#include <cassert>
#include <cerrno>
#include <cstdarg>
#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {
constexpr std::uint64_t kStartTime = 424242;
constexpr const char* kBootId = "test-runtime-acquisition-boot";
int ownerPid = 0;
int ownerChannel = -1;
bool identityReadable = true;
bool bootReadable = true;
int probeError = 0;
bool injectMarkerOnOpen = false;
bool exitOwnerDuringRead = false;
std::string injectedPath;

std::string contents(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  assert(input);
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

void writeFile(const std::filesystem::path& path, const std::string& value) {
  std::ofstream output(path);
  assert(output);
  output << value;
  output.close();
  assert(output);
}

void selectSlot(const std::string& slot) {
  assert(setenv("OPENPUZZLE_EXECUTION_SLOT", slot.c_str(), 1) == 0);
}

void saveMarker(const std::string& boot = kBootId,
                std::uint64_t start = kStartTime, int pid = ownerPid) {
  writeFile(openpuzzle::ClientRuntimeControl::pidPath(),
            std::to_string(pid) + '\n' + boot + '\n' +
                std::to_string(start) + '\n');
}

void finishOwner() {
  assert(ownerChannel >= 0);
  assert(close(ownerChannel) == 0);
  ownerChannel = -1;
  int status = 0;
  assert(waitpid(ownerPid, &status, 0) == ownerPid);
  assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

void startOwner() {
  int channel[2];
  assert(pipe(channel) == 0);
  ownerPid = fork();
  assert(ownerPid >= 0);
  if (ownerPid == 0) {
    close(channel[1]);
    char value;
    while (read(channel[0], &value, 1) < 0 && errno == EINTR) {}
    _exit(0);
  }
  assert(close(channel[0]) == 0);
  ownerChannel = channel[1];
}

std::string saveState(const std::string& slot,
                      const std::filesystem::path& home) {
  openpuzzle::client::ClientExecutionState state;
  state.active = true;
  state.assignmentId = "assignment-" + slot;
  state.clientId = "client";
  state.puzzle = 71;
  state.rangeId = 1;
  state.pid = ownerPid;
  state.bootId = kBootId;
  state.processStartTime = kStartTime;
  state.engine = "bitcrack";
  state.workspace = (home / "workspace").string();
  assert(openpuzzle::client::ClientStateStore::save(state, slot));
  return contents(openpuzzle::client::ClientStateStore::path(slot));
}

// All lifecycle callbacks except acquisition are local stubs. A refused
// acquisition must return before inspecting, synchronizing or requesting work.
void assertContinuousStartBlocked() {
  using namespace openpuzzle;
  int sideEffects = 0;
  ClientRuntimeDependencies d;
  d.sync = [&](const std::string&) { ++sideEffects; return client::ExecutionSyncResult{}; };
  d.heartbeat = [&](const std::string&) { ++sideEffects; return client::ClientHeartbeatResult{}; };
  d.stopExecution = [&](const std::string&) { ++sideEffects; return false; };
  d.reportSolution = [&](const std::string&, const std::string&, const std::string&, std::string&) { ++sideEffects; return false; };
  d.finalizeAssignment = [&](const std::string&, const std::string&, const std::string&, int, const std::string&, const std::string&, std::string&) { ++sideEffects; return client::AssignmentUploadStatus::Uploaded; };
  d.finalKeysChecked = [&](const std::string&) { ++sideEffects; return std::string{}; };
  d.removeState = [&] { ++sideEffects; return false; };
  d.hasState = [&] { ++sideEffects; return false; };
  d.acquireRuntime = [] { return ClientRuntimeControl::acquire(); };
  d.releaseRuntime = [&] { ++sideEffects; };
  d.stopRequested = [&] { ++sideEffects; return true; };
  d.safeStopRequested = [&] { ++sideEffects; return false; };
  d.clearSafeStop = [&] { ++sideEffects; return true; };
  d.prepareSignals = [&] { ++sideEffects; };
  d.sleep = [&](std::chrono::seconds) { ++sideEffects; };
  const ClientRuntime runtime(std::move(d));
  assert(runtime.runContinuous("https://unused.example.invalid", [&] {
    ++sideEffects;
    return ClientIterationResult{1};
  }) == 1);
  assert(sideEffects == 0);
}
}

// Fault injection is confined to this executable; production code and public
// interfaces retain the real boot/process reads and exclusive file creation.
extern "C" std::optional<openpuzzle::LinuxProcessIdentity::StartTime>
__wrap__ZN10openpuzzle20LinuxProcessIdentity9startTimeEi(int pid) {
  if (pid == ownerPid) {
    if (exitOwnerDuringRead) {
      exitOwnerDuringRead = false;
      finishOwner();
      return std::nullopt;
    }
    if (!identityReadable) return std::nullopt;
  }
  return pid > 0 ? std::optional<std::uint64_t>(kStartTime) : std::nullopt;
}

extern "C" std::string
__wrap__ZN10openpuzzle6client16ClientStateStore13currentBootIdB5cxx11Ev() {
  return bootReadable ? kBootId : "";
}

extern "C" int __real_kill(pid_t, int);
extern "C" int __wrap_kill(pid_t pid, int signal) {
  assert(signal == 0); // This executable never sends a stop signal.
  if (pid == ownerPid && probeError != 0) {
    errno = probeError;
    return -1;
  }
  return __real_kill(pid, signal);
}

extern "C" int __real_open(const char*, int, ...);
extern "C" int __wrap_open(const char* path, int flags, ...) {
  mode_t mode = 0;
  if (flags & O_CREAT) {
    va_list arguments;
    va_start(arguments, flags);
    mode = static_cast<mode_t>(va_arg(arguments, int));
    va_end(arguments);
  }
  if (injectMarkerOnOpen && injectedPath == path &&
      (flags & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL)) {
    injectMarkerOnOpen = false;
    saveMarker();
    writeFile(openpuzzle::ClientRuntimeControl::safeStopPath(), "requested\n");
  }
  return flags & O_CREAT ? __real_open(path, flags, mode) : __real_open(path, flags);
}

int main() {
  using openpuzzle::ClientRuntimeControl;
  using openpuzzle::client::ClientStateStore;
  const auto pattern = (std::filesystem::temp_directory_path() /
                        "openpuzzle-acquire-failure-XXXXXX").string();
  std::vector<char> temporary(pattern.begin(), pattern.end());
  temporary.push_back('\0');
  char* directory = mkdtemp(temporary.data());
  assert(directory);
  const std::filesystem::path home(directory);
  assert(setenv("HOME", home.c_str(), 1) == 0);
  startOwner();
  assert(__real_kill(ownerPid, 0) == 0);

  selectSlot("opencl-1");
  assert(ClientRuntimeControl::acquire());
  assert(ClientRuntimeControl::requestSafeStop());
  const auto peerMarker = contents(ClientRuntimeControl::pidPath());
  const auto peerRequest = contents(ClientRuntimeControl::safeStopPath());
  const auto peerState = saveState("opencl-1", home);

  selectSlot("cuda-0");
  saveMarker();
  writeFile(ClientRuntimeControl::safeStopPath(), "requested\n");
  const auto marker = contents(ClientRuntimeControl::pidPath());
  const auto request = contents(ClientRuntimeControl::safeStopPath());
  const auto state = saveState("cuda-0", home);
  const auto preserved = [&] {
    assert(contents(ClientRuntimeControl::pidPath()) == marker);
    assert(contents(ClientRuntimeControl::safeStopPath()) == request);
    assert(contents(ClientStateStore::path()) == state);
    assert(contents(ClientRuntimeControl::pidPath("opencl-1")) == peerMarker);
    assert(contents(ClientRuntimeControl::safeStopPath("opencl-1")) == peerRequest);
    assert(contents(ClientStateStore::path("opencl-1")) == peerState);
    assert(__real_kill(ownerPid, 0) == 0);
  };
  assert(ClientRuntimeControl::running());
  assert(!ClientRuntimeControl::acquire());
  preserved();

  identityReadable = false;
  assert(!ClientRuntimeControl::running()); // Uncertain is not verified running.
  assert(!ClientRuntimeControl::acquire());
  assert(!ClientRuntimeControl::acquire());
  assertContinuousStartBlocked();
  preserved();
  identityReadable = true;
  bootReadable = false;
  assert(!ClientRuntimeControl::acquire());
  preserved();
  bootReadable = true;

  for (const int error : {EPERM, EIO}) {
    probeError = error;
    assert(!ClientRuntimeControl::acquire());
    preserved();
  }
  probeError = 0;

  // Another owner appears between the first inspection and O_EXCL creation.
  // Both unreadable identity and readable liveness must survive the EEXIST path.
  for (const bool readable : {false, true}) {
    std::filesystem::remove(ClientRuntimeControl::pidPath());
    injectedPath = ClientRuntimeControl::pidPath().string();
    injectMarkerOnOpen = true;
    identityReadable = readable;
    assert(!ClientRuntimeControl::acquire());
    assert(!injectMarkerOnOpen);
    preserved();
  }
  identityReadable = true;

  // Positive stale evidence must still permit acquisition, including legacy
  // PID-only and PID+boot markers, different boots and same-boot PID reuse.
  for (const std::string& stale : {
           std::to_string(ownerPid) + "\n",
           std::to_string(ownerPid) + '\n' + kBootId + "\n",
           std::to_string(ownerPid) + "\nold-boot\n424242\n",
           std::to_string(ownerPid) + '\n' + kBootId + "\n424243\n"}) {
    writeFile(ClientRuntimeControl::pidPath(), stale);
    writeFile(ClientRuntimeControl::safeStopPath(), request);
    assert(ClientRuntimeControl::acquire());
    assert(ClientRuntimeControl::runtimePid() == static_cast<int>(getpid()));
    assert(!ClientRuntimeControl::safeStopRequested());
    assert(contents(ClientStateStore::path()) == state);
    assert(ClientRuntimeControl::release());
  }

  // A process can exit during the failed stat read. The second existence probe
  // must allow stale cleanup once exit is established, without a signal.
  saveMarker();
  writeFile(ClientRuntimeControl::safeStopPath(), request);
  exitOwnerDuringRead = true;
  assert(ClientRuntimeControl::acquire());
  assert(!exitOwnerDuringRead && ownerChannel == -1);
  assert(ClientRuntimeControl::release());

  saveMarker(); // The saved owner has exited and has been reaped.
  writeFile(ClientRuntimeControl::safeStopPath(), request);
  identityReadable = false;
  assert(ClientRuntimeControl::acquire());
  assert(!ClientRuntimeControl::safeStopRequested());
  assert(contents(ClientStateStore::path()) == state);
  assert(contents(ClientRuntimeControl::pidPath("opencl-1")) == peerMarker);
  assert(contents(ClientStateStore::path("opencl-1")) == peerState);
  assert(ClientRuntimeControl::release());
  selectSlot("opencl-1");
  assert(ClientRuntimeControl::release());
  std::filesystem::remove_all(home);
  std::cout << "RuntimeAcquireFailureTests passed\n";
}

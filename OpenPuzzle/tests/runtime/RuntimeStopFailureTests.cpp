#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/runtime/ClientRuntimeControl.hpp"
#include "openpuzzle/runtime/LinuxProcessIdentity.hpp"
#include "openpuzzle/runtime/RunSession.hpp"

#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>
#include <unistd.h>

namespace {
constexpr std::uint64_t kStartTime = 424242;
bool identityReadable = true;
int signalError = EPERM;
int signalCalls = 0;
int successfulSend = -1;

std::string contents(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  assert(input);
  return std::string(std::istreambuf_iterator<char>(input),
                     std::istreambuf_iterator<char>());
}

void selectSlot(const std::string& slot) {
  assert(setenv("OPENPUZZLE_EXECUTION_SLOT", slot.c_str(), 1) == 0);
}

std::string saveState(const std::string& slot,
                      const std::filesystem::path& home) {
  openpuzzle::client::ClientExecutionState state;
  state.active = true;
  state.assignmentId = "assignment-" + slot;
  state.clientId = "client";
  state.puzzle = 71;
  state.rangeId = 1;
  state.pid = 999999999;
  state.bootId = "previous-boot";
  state.processStartTime = 1;
  state.engine = "bitcrack";
  state.workspace = (home / "workspace").string();
  assert(openpuzzle::client::ClientStateStore::save(state, slot));
  return contents(openpuzzle::client::ClientStateStore::path(slot));
}
}

// These linker wrappers are local to this test executable. No actual signal
// is sent and no GPU, assignment server or engine process is needed.
extern "C" std::optional<openpuzzle::LinuxProcessIdentity::StartTime>
__wrap__ZN10openpuzzle20LinuxProcessIdentity9startTimeEi(int pid) {
  if (pid <= 0 || !identityReadable) {
    return std::nullopt;
  }
  return kStartTime;
}

extern "C" bool
__wrap__ZN10openpuzzle20LinuxProcessIdentity15signalIfMatchesEimi(
    int pid, std::uint64_t startTime, int signal) {
  assert(pid == static_cast<int>(getpid()));
  assert(startTime == kStartTime);
  assert(signal == SIGTERM);
  const bool success = signalCalls++ == successfulSend;
  errno = success ? 0 : signalError;
  return success;
}

int main() {
  using openpuzzle::ClientRuntimeControl;
  using openpuzzle::client::ClientStateStore;
  const auto pattern = (std::filesystem::temp_directory_path() /
                        "openpuzzle-stop-failure-XXXXXX").string();
  std::vector<char> temporary(pattern.begin(), pattern.end());
  temporary.push_back('\0');
  char* directory = mkdtemp(temporary.data());
  assert(directory);
  const std::filesystem::path home(directory);
  assert(setenv("HOME", home.c_str(), 1) == 0);

  selectSlot("gpu");
  assert(ClientRuntimeControl::acquire());
  const auto peerMarker = contents(ClientRuntimeControl::pidPath());
  const auto peerState = saveState("gpu", home);

  const std::vector<std::pair<std::string, int>> failures = {
      {"cuda-0", ENOSYS}, {"cuda-1", EPERM}, {"opencl-1", ESRCH},
  };
  for (const auto& [slot, failure] : failures) {
    selectSlot(slot);
    assert(ClientRuntimeControl::acquire());
    assert(ClientRuntimeControl::requestSafeStop());
    const auto marker = contents(ClientRuntimeControl::pidPath());
    const auto request = contents(ClientRuntimeControl::safeStopPath());
    const auto state = saveState(slot, home);
    signalError = failure;
    signalCalls = 0;
    assert(!ClientRuntimeControl::requestStop(slot));
    assert(signalCalls == 1);
    assert(ClientRuntimeControl::running(slot));
    assert(contents(ClientRuntimeControl::pidPath()) == marker);
    assert(contents(ClientRuntimeControl::safeStopPath()) == request);
    assert(contents(ClientStateStore::path()) == state);
    assert(!ClientRuntimeControl::requestStop(slot));
    assert(signalCalls == 2);
    assert(!ClientRuntimeControl::acquire());
    assert(ClientRuntimeControl::safeStopRequested());
    assert(contents(ClientRuntimeControl::pidPath("gpu")) == peerMarker);
    assert(contents(ClientStateStore::path("gpu")) == peerState);
    assert(ClientRuntimeControl::release());
    assert(ClientRuntimeControl::clearSafeStop());
    assert(ClientStateStore::remove());
  }
  selectSlot("gpu");
  assert(ClientRuntimeControl::release());
  assert(ClientStateStore::remove());

  // Exercise the public stop command, including its legacy engine fallback.
  // A retained runtime marker must prevent assignment removal in that fallback.
  const openpuzzle::RunSession session;
  selectSlot("primary");
  assert(ClientRuntimeControl::acquire());
  const auto primaryMarker = contents(ClientRuntimeControl::pidPath());
  const auto primaryState = saveState("primary", home);
  assert(session.run({"stop"}) == 1);
  assert(contents(ClientRuntimeControl::pidPath()) == primaryMarker);
  assert(contents(ClientStateStore::path()) == primaryState);
  identityReadable = false;
  signalCalls = 0;
  assert(session.run({"stop"}) == 1);
  assert(signalCalls == 0);
  assert(contents(ClientRuntimeControl::pidPath()) == primaryMarker);
  assert(contents(ClientStateStore::path()) == primaryState);
  identityReadable = true;
  assert(ClientRuntimeControl::release());
  assert(ClientStateStore::remove());

  selectSlot("cuda-0");
  assert(ClientRuntimeControl::acquire());
  const auto cudaMarker = contents(ClientRuntimeControl::pidPath());
  const auto cudaState = saveState("cuda-0", home);
  const auto cpuState = saveState("cpu", home);
  selectSlot("primary");
  assert(session.run({"stop"}) == 1);
  assert(contents(ClientRuntimeControl::pidPath("cuda-0")) == cudaMarker);
  assert(contents(ClientStateStore::path("cuda-0")) == cudaState);
  assert(contents(ClientStateStore::path("cpu")) == cpuState);

  // A successful request for one slot must not hide another slot's failure.
  selectSlot("gpu");
  assert(ClientRuntimeControl::acquire());
  selectSlot("primary");
  signalCalls = 0;
  successfulSend = 1; // gpu fails; cuda-0 reports success, without a real signal.
  assert(session.run({"stop"}) == 1);
  assert(signalCalls == 2);
  assert(ClientRuntimeControl::running("gpu"));
  assert(ClientRuntimeControl::running("cuda-0"));
  assert(contents(ClientStateStore::path("cuda-0")) == cudaState);
  assert(contents(ClientStateStore::path("cpu")) == cpuState);
  selectSlot("gpu");
  assert(ClientRuntimeControl::release());
  selectSlot("cuda-0");
  assert(ClientRuntimeControl::release());
  std::filesystem::remove_all(home);
  std::cout << "RuntimeStopFailureTests passed\n";
}

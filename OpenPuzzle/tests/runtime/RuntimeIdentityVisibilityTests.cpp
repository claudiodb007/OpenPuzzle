#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/core/commands/UpdateCommand.hpp"
#include "openpuzzle/hardware/GpuInfo.hpp"
#include "openpuzzle/hardware/GpuTelemetry.hpp"
#include "openpuzzle/runtime/ClientRuntimeControl.hpp"
#include "openpuzzle/runtime/RunSession.hpp"
#include "openpuzzle/services/DoctorService.hpp"

#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {
using namespace openpuzzle;
constexpr std::uint64_t startTime = 424242;
constexpr const char* bootId = "test-visibility-boot";
bool readable = false;
bool bootReadable = true;
bool failAfterFirst = false;
int identityReads = 0;
int unreadablePid = 0;
int probeError = 0;
int failures = 0;
int child = 0;

void check(bool condition, const std::string& description) {
  if (!condition) {
    std::cerr << "FAIL: " << description << '\n';
    ++failures;
  }
}

void writeFile(const std::filesystem::path& path, const std::string& value) {
  std::ofstream out(path);
  assert(out);
  out << value;
  out.close();
  assert(out);
}

std::string contents(const std::filesystem::path& path) {
  std::ifstream input(path);
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void marker(const std::string& slot, const std::string& boot = bootId,
            std::uint64_t start = startTime) {
  writeFile(ClientRuntimeControl::pidPath(slot),
            std::to_string(child) + '\n' + boot + '\n' + std::to_string(start) + '\n');
}

struct Output {
  int code;
  std::string out;
  std::string err;
};

template<class F> Output capture(F function) {
  std::ostringstream out, err;
  auto* oldOut = std::cout.rdbuf(out.rdbuf());
  auto* oldErr = std::cerr.rdbuf(err.rdbuf());
  const int code = function();
  std::cout.rdbuf(oldOut);
  std::cerr.rdbuf(oldErr);
  return {code, out.str(), err.str()};
}

void checkBlockedUpdate(const std::vector<std::string>& args = {}) {
  // Empty PATH is deliberate: refusal must happen before command discovery,
  // network access, package download or installation.
  const auto result = capture([&] { return UpdateCommand{}.run(args); });
  check(result.code == 1, "updater refuses an occupied or uncertain slot");
  check(result.err.find(args.empty() ? "OP-UPDATE-001" : "OP-UPDATE-006") != std::string::npos,
        "updater refuses before discovering external tools");
}

std::string doctorRuntime() {
  const auto result = capture([] { return DoctorService{}.execute({"--offline"}); });
  const auto section = result.out.find("Runtime state\n");
  check(section != std::string::npos, "doctor reports runtime section");
  return section == std::string::npos ? "" : result.out.substr(section);
}

void saveState(const std::filesystem::path& home, const std::string& slot) {
  client::ClientExecutionState state;
  state.active = true;
  state.assignmentId = "visibility-assignment";
  state.clientId = "local-client";
  state.puzzle = 71;
  state.rangeId = 1;
  state.pid = child;
  state.bootId = bootId;
  state.processStartTime = startTime;
  state.engine = "bitcrack";
  state.workspace = (home / "empty-workspace").string();
  assert(client::ClientStateStore::save(state, slot));
}
} // namespace

// Wrappers affect this test only. A real child stays alive through a pipe;
// GPU discovery/telemetry is empty and no termination signal is permitted.
extern "C" std::optional<std::uint64_t>
__wrap__ZN10openpuzzle20LinuxProcessIdentity9startTimeEi(int pid) {
  ++identityReads;
  return readable && pid != unreadablePid && (!failAfterFirst || identityReads == 1)
      ? std::optional<std::uint64_t>(startTime) : std::nullopt;
}
extern "C" std::string
__wrap__ZN10openpuzzle6client16ClientStateStore13currentBootIdB5cxx11Ev() {
  return bootReadable ? bootId : "";
}
extern "C" int __real_kill(int, int);
extern "C" int __wrap_kill(int pid, int signal) {
  assert(signal == 0);
  if (probeError) { errno = probeError; return -1; }
  return __real_kill(pid, signal);
}
extern "C" std::vector<GpuInfo>
__wrap__ZN10openpuzzle10GpuManager12listCudaGpusEv() { return {}; }
extern "C" std::vector<GpuInfo>
__wrap__ZN10openpuzzle10GpuManager14listOpenClGpusEv() { return {}; }
extern "C" std::vector<GpuTelemetrySnapshot>
__wrap__ZN10openpuzzle12GpuTelemetry7readAllEv() { return {}; }

int main() {
  char temporary[] = "/tmp/openpuzzle-visibility-tests-XXXXXX";
  assert(mkdtemp(temporary));
  const std::filesystem::path home(temporary);
  assert(setenv("HOME", temporary, 1) == 0);
  assert(setenv("XDG_DATA_HOME", (home / ".local/share").c_str(), 1) == 0);
  assert(setenv("PATH", "", 1) == 0);
  assert(setenv("OPENPUZZLE_EXECUTION_SLOT", "primary", 1) == 0);
  int channel[2];
  assert(pipe(channel) == 0);
  child = fork();
  assert(child >= 0);
  if (child == 0) {
    close(channel[1]);
    char byte;
    while (read(channel[0], &byte, 1) < 0 && errno == EINTR) {}
    _exit(0);
  }
  assert(close(channel[0]) == 0);
  std::filesystem::create_directories(ClientRuntimeControl::pidPath().parent_path());

  // An alive supervisor with unreadable identity is never reported idle/stale.
  for (const auto& slot : {"primary", "cuda-0", "opencl-1"}) {
    marker(slot);
    const auto before = contents(ClientRuntimeControl::pidPath(slot));
    const auto safe = capture([] { return RunSession{}.run({"safestop"}); });
    check(safe.code == 1 && safe.out.find("No active") == std::string::npos,
          "safe stop reports uncertain " + std::string(slot));
    const auto status = capture([] { return RunSession{}.run({"status"}); });
    check(status.out.find("identity unavailable") != std::string::npos &&
              status.out.find("idle") == std::string::npos,
          "status exposes uncertain " + std::string(slot));
    const auto doctor = doctorRuntime();
    check(doctor.find("Unknown identities. 1") != std::string::npos &&
              doctor.find("Stale PID files.... 0") != std::string::npos &&
              doctor.find("OP-DOCTOR-006") != std::string::npos,
          "doctor distinguishes uncertainty from stale " + std::string(slot));
    checkBlockedUpdate();
    checkBlockedUpdate({"--safe"});
    check(contents(ClientRuntimeControl::pidPath(slot)) == before,
          "read-only inspection and refusal preserve runtime marker");
    std::filesystem::remove(ClientRuntimeControl::pidPath(slot));
  }

  // A state-only engine with unknown identity must also block installation.
  saveState(home, "opencl-1");
  const auto stateBefore = contents(client::ClientStateStore::path("opencl-1"));
  auto status = capture([] { return RunSession{}.run({"status"}); });
  check(status.out.find("identity unavailable") != std::string::npos &&
            status.out.find("stopped") == std::string::npos,
        "status does not call an uncertain engine stopped");
  checkBlockedUpdate();
  checkBlockedUpdate({"--safe"});
  check(doctorRuntime().find("Unknown identities. 1") != std::string::npos,
        "doctor includes uncertainty in preserved engine state");
  check(contents(client::ClientStateStore::path("opencl-1")) == stateBefore,
        "inspection preserves assignment state");
  std::filesystem::remove(client::ClientStateStore::path("opencl-1"));

  // Known dynamic runtimes are discovered by both doctor and updater.
  readable = true;
  marker("cuda-0");
  checkBlockedUpdate();
  check(doctorRuntime().find("Active runtimes.... 1") != std::string::npos,
        "doctor counts dynamic runtime");
  marker("primary");
  const auto safe = capture([] { return RunSession{}.run({"safestop"}); });
  check(safe.code == 0 && safe.out.find("primary") != std::string::npos &&
            safe.out.find("cuda-0") != std::string::npos,
        "safe stop includes primary alongside dynamic workers");

  // Repetition and a read failure after creation never truncate/delete a request.
  const auto request = ClientRuntimeControl::safeStopPath("cuda-0");
  writeFile(request, "earlier request bytes\n");
  check(ClientRuntimeControl::requestSafeStop("cuda-0") &&
            contents(request) == "earlier request bytes\n",
        "safe-stop repetition preserves existing request bytes");
  failAfterFirst = true;
  identityReads = 0;
  check(!ClientRuntimeControl::requestSafeStop("cuda-0") &&
            contents(request) == "earlier request bytes\n",
        "read failure preserves pre-existing safe stop");
  std::filesystem::remove(request);
  identityReads = 0;
  check(!ClientRuntimeControl::requestSafeStop("cuda-0") &&
            std::filesystem::is_regular_file(request),
        "read failure after creating safe stop retains request");
  failAfterFirst = false;
  readable = false;
  const auto partial = capture([] { return RunSession{}.run({"safestop"}); });
  check(partial.code == 1 && std::filesystem::is_regular_file(request),
        "uncertain safe stop never rolls back existing requests");

  // Partial failure must not undo a successfully requested peer or claim that
  // all new assignments have been blocked. Primary and worker are both inspected.
  readable = true;
  unreadablePid = child;
  writeFile(ClientRuntimeControl::pidPath("primary"),
            std::to_string(getpid()) + '\n' + bootId + "\n424242\n");
  std::filesystem::remove(ClientRuntimeControl::safeStopPath("primary"));
  const auto mixed = capture([] { return RunSession{}.run({"safestop"}); });
  check(mixed.code == 1 && mixed.out.find("New assignments..... blocked") == std::string::npos &&
            std::filesystem::is_regular_file(ClientRuntimeControl::safeStopPath("primary")) &&
            std::filesystem::is_regular_file(request),
        "partial safe stop preserves both peer requests and reports failure");
  status = capture([] { return RunSession{}.run({"status"}); });
  check(status.out.find("primary") != std::string::npos &&
            status.out.find("cuda-0") != std::string::npos &&
            status.out.find("identity unavailable") != std::string::npos,
        "status reports verified primary alongside uncertain dynamic worker");
  unreadablePid = 0;
  marker("primary");

  // Boot read failure and probe failures remain unknown, even for an alive PID.
  readable = true;
  bootReadable = false;
  checkBlockedUpdate();
  bootReadable = true;
  for (const auto error : {EIO, EPERM}) {
    probeError = error;
    readable = false;
    checkBlockedUpdate();
  }
  probeError = 0;
  readable = true;
  marker("primary", "different-boot");
  marker("cuda-0", bootId, startTime + 1);
  status = capture([] { return RunSession{}.run({"status"}); });
  check(status.out.find("idle") != std::string::npos,
        "different boot and reused PID still count as inactive");
  auto inactive = capture([] { return UpdateCommand{}.run({}); });
  check(inactive.err.find("OP-UPDATE-002") != std::string::npos,
        "confirmed stale identities do not block updater preflight");

  assert(close(channel[1]) == 0);
  int childStatus = 0;
  assert(waitpid(child, &childStatus, 0) == child);
  assert(WIFEXITED(childStatus) && WEXITSTATUS(childStatus) == 0);
  marker("primary");
  readable = false;
  status = capture([] { return RunSession{}.run({"status"}); });
  check(status.out.find("idle") != std::string::npos,
        "confirmed missing process still counts as inactive");
  std::filesystem::remove_all(home);
  if (failures) { std::cerr << failures << " visibility checks failed\n"; return 1; }
  std::cout << "RuntimeIdentityVisibilityTests passed\n";
  return 0;
}

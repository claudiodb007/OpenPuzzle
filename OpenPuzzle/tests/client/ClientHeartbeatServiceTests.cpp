#include "openpuzzle/client/ClientHeartbeatService.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/runtime/LinuxProcessIdentity.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <regex>
#include <string>
#include <unistd.h>

using namespace openpuzzle::client;

namespace {

bool isUuidV4(
    const std::string& value) {
  const std::regex expression(
      "^[0-9a-f]{8}-"
      "[0-9a-f]{4}-"
      "4[0-9a-f]{3}-"
      "[89ab][0-9a-f]{3}-"
      "[0-9a-f]{12}$",
      std::regex::icase);

  return std::regex_match(
      value,
      expression);
}

} // namespace

int main() {
  const char* originalHome =
      std::getenv("HOME");

  const bool hadHome =
      originalHome != nullptr;

  const std::string savedHome =
      hadHome
          ? originalHome
          : "";

  const auto temporaryHome =
      std::filesystem::temp_directory_path() /
      (
          "openpuzzle-client-heartbeat-" +
          std::to_string(getpid())
      );

  std::filesystem::remove_all(
      temporaryHome);

  std::filesystem::create_directories(
      temporaryHome);

  assert(
      setenv(
          "HOME",
          temporaryHome.string().c_str(),
          1) == 0);

  const auto heartbeat =
      ClientHeartbeatService::
          collectLocalHeartbeat();

  assert(heartbeat.valid());
  assert(isUuidV4(heartbeat.clientId));
  assert(!heartbeat.version.empty());
  assert(!heartbeat.platform.empty());
  assert(heartbeat.status == "idle");
  assert(heartbeat.activeEngine.empty());
  assert(heartbeat.activeBackend.empty());

  assert(!heartbeat.cpu.name.empty());
  assert(heartbeat.cpu.cores > 0);
  assert(heartbeat.cpu.threads > 0);

  for (const auto& gpu :
       heartbeat.gpus) {
    assert(gpu.valid());
    assert(!gpu.name.empty());
    assert(!gpu.backend.empty());
  }

  assert(
      heartbeat.engines.size() == 3);

  for (const auto& engine :
       heartbeat.engines) {
    assert(engine.valid());

    if (engine.available) {
      assert(engine.installed);
    }
  }

  ClientExecutionState cpuState;

  cpuState.active = true;
  cpuState.assignmentId =
      "11111111-1111-4111-8111-111111111111";
  cpuState.clientId = heartbeat.clientId;
  cpuState.puzzle = 71;
  cpuState.rangeId = 238;
  cpuState.pid = static_cast<int>(getpid());
  cpuState.bootId =
      ClientStateStore::currentBootId();
  const auto cpuStateStartTime =
      openpuzzle::LinuxProcessIdentity::startTime(cpuState.pid);
  assert(cpuStateStartTime);
  cpuState.processStartTime = *cpuStateStartTime;
  cpuState.threads = 8;
  cpuState.target =
      "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU";
  cpuState.start = "400000000000000000";
  cpuState.end = "4000000000FFFFFFFF";
  cpuState.engine = "KeyHunt";
  cpuState.backend = "CPU";
  cpuState.workspace =
      temporaryHome.string();
  cpuState.command =
      "keyhunt -t 8";

  assert(
      ClientStateStore::save(
          cpuState,
          "cpu"));

  ClientExecutionState gpuState =
      cpuState;
  gpuState.assignmentId =
      "33333333-3333-4333-8333-333333333333";
  gpuState.rangeId = 239;
  gpuState.threads = 0;
  gpuState.engine = "BitCrack";
  gpuState.backend = "CUDA";
  gpuState.command = "cuBitCrack";

  assert(
      ClientStateStore::save(
          gpuState,
          "gpu"));

  const auto cpuHeartbeat =
      ClientHeartbeatService::
          collectLocalHeartbeat();

  assert(cpuHeartbeat.valid());
  assert(cpuHeartbeat.status == "running");
  assert(cpuHeartbeat.activeEngine == "BitCrack");
  assert(cpuHeartbeat.activeBackend == "CUDA");
  assert(cpuHeartbeat.activeBackends.size() == 2);
  assert(
      std::find(
          cpuHeartbeat.activeBackends.begin(),
          cpuHeartbeat.activeBackends.end(),
          "CUDA") !=
      cpuHeartbeat.activeBackends.end());
  assert(
      std::find(
          cpuHeartbeat.activeBackends.begin(),
          cpuHeartbeat.activeBackends.end(),
          "CPU") !=
      cpuHeartbeat.activeBackends.end());
  assert(cpuHeartbeat.cpu.threads == 8);

  /*
   * Um PID numericamente válido pertencente a outro boot
   * não pode ser anunciado ao servidor como execução ativa.
   */
  ClientExecutionState staleState =
      cpuState;

  staleState.assignmentId =
      "55555555-5555-4555-8555-555555555555";

  staleState.rangeId = 240;

  staleState.bootId =
      "00000000-0000-0000-0000-000000000000";

  staleState.engine = "BitCrack";
  staleState.backend = "OpenCL";
  staleState.threads = 0;

  assert(
      ClientStateStore::save(
          staleState,
          "opencl"));

  const auto heartbeatWithStalePid =
      ClientHeartbeatService::
          collectLocalHeartbeat();

  assert(
      heartbeatWithStalePid.status ==
      "running");

  assert(
      std::find(
          heartbeatWithStalePid.activeBackends.begin(),
          heartbeatWithStalePid.activeBackends.end(),
          "OpenCL") ==
      heartbeatWithStalePid.activeBackends.end());

  assert(
      ClientStateStore::remove(
          "opencl"));

  /*
   * Estado legado <= 1.0.16 sem boot_id também não pode
   * ser tratado como execução ativa pelo heartbeat.
   */
  ClientExecutionState legacyState =
      cpuState;

  legacyState.assignmentId =
      "66666666-6666-4666-8666-666666666666";

  legacyState.rangeId = 241;
  legacyState.bootId.clear();
  legacyState.engine = "BitCrack";
  legacyState.backend = "OpenCL";
  legacyState.threads = 0;

  assert(
      ClientStateStore::save(
          legacyState,
          "opencl"));

  const auto heartbeatWithLegacyPid =
      ClientHeartbeatService::
          collectLocalHeartbeat();

  assert(
      std::find(
          heartbeatWithLegacyPid.activeBackends.begin(),
          heartbeatWithLegacyPid.activeBackends.end(),
          "OpenCL") ==
      heartbeatWithLegacyPid.activeBackends.end());

  assert(
      ClientStateStore::remove(
          "opencl"));

  assert(
      ClientStateStore::remove("gpu"));
  assert(
      ClientStateStore::remove("cpu"));

  ClientHeartbeatService service;

  const auto failed =
      service.send(
          "http://127.0.0.1:1");

  assert(!failed.success);
  assert(!failed.error.empty());

  std::filesystem::remove_all(
      temporaryHome);

  if (hadHome) {
    assert(
        setenv(
            "HOME",
            savedHome.c_str(),
            1) == 0);
  } else {
    assert(
        unsetenv("HOME") == 0);
  }

  std::cout
      << "ClientHeartbeatServiceTests passed\n";

  return 0;
}

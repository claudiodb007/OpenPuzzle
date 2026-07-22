#include "openpuzzle/client/ClientHeartbeatService.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"

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
          cpuState));

  const auto cpuHeartbeat =
      ClientHeartbeatService::
          collectLocalHeartbeat();

  assert(cpuHeartbeat.valid());
  assert(cpuHeartbeat.status == "running");
  assert(cpuHeartbeat.activeEngine == "KeyHunt");
  assert(cpuHeartbeat.activeBackend == "CPU");
  assert(cpuHeartbeat.cpu.threads == 8);

  assert(
      ClientStateStore::remove());

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

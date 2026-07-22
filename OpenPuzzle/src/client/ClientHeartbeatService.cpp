#include "openpuzzle/client/ClientHeartbeatService.hpp"

#include "openpuzzle/client/ClientIdentity.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/client/HttpRangeClient.hpp"
#include "openpuzzle/engines/EngineManager.hpp"
#include "openpuzzle/hardware/GpuManager.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <thread>
#include <unistd.h>

namespace openpuzzle::client {

namespace {

std::string trim(
    std::string value) {
  while (!value.empty() &&
         std::isspace(
             static_cast<unsigned char>(
                 value.back()))) {
    value.pop_back();
  }

  std::size_t begin = 0;

  while (begin < value.size() &&
         std::isspace(
             static_cast<unsigned char>(
                 value[begin]))) {
    ++begin;
  }

  return value.substr(begin);
}

std::string unquote(
    std::string value) {
  value = trim(
      std::move(value));

  if (value.size() >= 2 &&
      value.front() == '"' &&
      value.back() == '"') {
    return value.substr(
        1,
        value.size() - 2);
  }

  return value;
}

bool processExists(
    int pid) {
  if (pid <= 0) {
    return false;
  }

  if (kill(pid, 0) == 0) {
    return true;
  }

  return errno == EPERM;
}

} // namespace

std::string
ClientHeartbeatService::platform() {
  std::ifstream input(
      "/etc/os-release");

  std::string line;

  while (std::getline(
      input,
      line)) {
    constexpr const char* prefix =
        "PRETTY_NAME=";

    if (line.rfind(prefix, 0) == 0) {
      const auto value =
          unquote(
              line.substr(12));

      if (!value.empty()) {
        return value;
      }
    }
  }

#if defined(__linux__)
  return "Linux";
#elif defined(__APPLE__)
  return "macOS";
#elif defined(_WIN32)
  return "Windows";
#else
  return "Unknown";
#endif
}

ClientCpuCapability
ClientHeartbeatService::cpu() {
  ClientCpuCapability result;

  std::ifstream input(
      "/proc/cpuinfo");

  std::string line;
  std::string physicalId;
  std::string coreId;

  std::set<std::string>
      physicalCores;

  while (std::getline(
      input,
      line)) {
    const auto separator =
        line.find(':');

    if (separator ==
        std::string::npos) {
      continue;
    }

    const auto key =
        trim(
            line.substr(
                0,
                separator));

    const auto value =
        trim(
            line.substr(
                separator + 1));

    if (key == "model name" &&
        result.name.empty()) {
      result.name = value;
    } else if (key == "physical id") {
      physicalId = value;
    } else if (key == "core id") {
      coreId = value;

      if (!physicalId.empty()) {
        physicalCores.insert(
            physicalId + ":" + coreId);
      }
    }
  }

  result.threads =
      static_cast<int>(
          std::thread::hardware_concurrency());

  if (!physicalCores.empty()) {
    result.cores =
        static_cast<int>(
            physicalCores.size());
  } else {
    result.cores =
        result.threads;
  }

  if (result.name.empty()) {
    result.name =
        "Unknown CPU";
  }

  if (result.cores <= 0) {
    result.cores = 1;
  }

  if (result.threads <= 0) {
    result.threads =
        result.cores;
  }

  return result;
}

std::vector<ClientGpuCapability>
ClientHeartbeatService::gpus() {
  std::vector<ClientGpuCapability>
      capabilities;

  for (const auto& gpu :
       GpuManager::listGpus()) {
    ClientGpuCapability capability;

    /*
     * O GpuManager atual deteta estas placas
     * através de nvidia-smi.
     *
     * Apenas backend, nome e memória são
     * incluídos na capacidade pública.
     */
    capability.backend =
        "CUDA";

    capability.name =
        gpu.name;

    capability.memoryMB =
        static_cast<std::uint64_t>(gpu.memoryMb);

    if (capability.valid()) {
      capabilities.push_back(
          std::move(capability));
    }
  }

  return capabilities;
}

std::vector<ClientEngineCapability>
ClientHeartbeatService::engines() {
  std::vector<ClientEngineCapability>
      capabilities;

  EngineManager manager;

  const auto makeCapability =
      [&manager](
          const std::string& name,
          const std::string& backend) {
        ClientEngineCapability capability;

        capability.name = name;
        capability.backend = backend;

        const auto executable =
            manager.resolveExecutable(
                name,
                backend);

        capability.installed =
            executable.has_value() &&
            !executable->empty() &&
            std::filesystem::is_regular_file(
                *executable);

        capability.available =
            capability.installed;

        return capability;
      };

  capabilities.push_back(
      makeCapability(
          "BitCrack",
          "CUDA"));

  capabilities.push_back(
      makeCapability(
          "BitCrack",
          "OpenCL"));

  capabilities.push_back(
      makeCapability(
          "KeyHunt",
          "CPU"));

  return capabilities;
}

std::string
ClientHeartbeatService::
executionStatus(
    std::string& activeEngine,
    std::string& activeBackend,
    int& activeCpuThreads) {
  activeEngine.clear();
  activeBackend.clear();
  activeCpuThreads = 0;

  const auto state =
      ClientStateStore::load();

  if (!state ||
      !processExists(state->pid)) {
    return "idle";
  }

  activeEngine = state->engine;
  activeBackend = state->backend;

  if (
      activeBackend == "CPU" ||
      activeBackend == "cpu") {
    activeCpuThreads =
        state->threads;
  }

  return "running";
}

ClientHeartbeat
ClientHeartbeatService::
collectLocalHeartbeat() {
  ClientHeartbeat heartbeat;

  heartbeat.clientId =
      ClientIdentity::loadOrCreate();

#ifdef OPENPUZZLE_VERSION
  heartbeat.version =
      OPENPUZZLE_VERSION;
#else
  heartbeat.version =
      "unknown";
#endif

  heartbeat.platform =
      platform();

  int activeCpuThreads = 0;

  heartbeat.status =
      executionStatus(
          heartbeat.activeEngine,
          heartbeat.activeBackend,
          activeCpuThreads);

  heartbeat.cpu =
      cpu();

  if (
      activeCpuThreads > 0 &&
      (
          heartbeat.activeBackend == "CPU" ||
          heartbeat.activeBackend == "cpu"
      )) {
    heartbeat.cpu.threads = activeCpuThreads;
  }

  heartbeat.gpus =
      gpus();

  heartbeat.engines =
      engines();

  return heartbeat;
}

ClientHeartbeatResult
ClientHeartbeatService::send(
    const std::string& serverUrl) const {
  ClientHeartbeatResult result;

  result.heartbeat =
      collectLocalHeartbeat();

  if (!result.heartbeat.valid()) {
    result.error =
        "Unable to collect valid client heartbeat";

    return result;
  }

  HttpRangeClient client(
      serverUrl);

  result.success =
      client.heartbeat(
          result.heartbeat);

  if (!result.success) {
    result.error =
        client.lastError();
  }

  return result;
}

} // namespace openpuzzle::client

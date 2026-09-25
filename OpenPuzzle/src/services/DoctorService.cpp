#include "openpuzzle/services/DoctorService.hpp"

#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/config/ConfigurationManager.hpp"
#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/hardware/GpuInfo.hpp"
#include "openpuzzle/hardware/GpuManager.hpp"
#include "openpuzzle/hardware/GpuTelemetry.hpp"
#include "openpuzzle/hardware/GpuThermalPolicy.hpp"
#include "openpuzzle/hardware/RusticlEnvironment.hpp"
#include "openpuzzle/models/Models.hpp"
#include "openpuzzle/runtime/ClientRuntimeControl.hpp"
#include "openpuzzle/tools/ToolManager.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace openpuzzle {

namespace {

struct DoctorSummary {
  int warnings = 0;
  int errors = 0;
};

struct ServerProbeResult {
  bool reachable = false;
  int httpStatus = 0;
};

std::filesystem::path homePath() {
  const char *home = std::getenv("HOME");

  return home && *home != '\0'
      ? std::filesystem::path(home)
      : std::filesystem::current_path();
}

std::filesystem::path dataPath() {
  return homePath() / ".local" / "share" / "OpenPuzzle";
}

bool hasArgument(const std::vector<std::string> &args,
                 const std::string &name) {
  for (const auto &argument : args) {
    if (argument == name) {
      return true;
    }
  }

  return false;
}

std::string coordinationServerUrl() {
  const char *environment = std::getenv("OPENPUZZLE_SERVER_URL");

  if (environment && *environment != '\0') {
    return environment;
  }

  return "https://claudiodb.com";
}

std::string shellQuote(const std::string &value) {
  std::string quoted = "'";

  for (const char character : value) {
    if (character == '\'') {
      quoted += "'\\''";
    } else {
      quoted += character;
    }
  }

  quoted += '\'';
  return quoted;
}

ServerProbeResult probeServer(const std::string &url) {
  const std::string command =
      "curl --silent --show-error --location --head "
      "--output /dev/null --connect-timeout 5 --max-time 10 "
      "--write-out '%{http_code}' " +
      shellQuote(url) +
      " 2>/dev/null";

  FILE *pipe = popen(command.c_str(), "r");
  if (!pipe) {
    return {};
  }

  std::string response;
  char buffer[32] = {};
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    response += buffer;
  }

  const int processStatus = pclose(pipe);
  int httpStatus = 0;

  try {
    httpStatus = std::stoi(response);
  } catch (...) {
    httpStatus = 0;
  }

  const bool curlSucceeded =
      processStatus != -1 &&
      WIFEXITED(processStatus) &&
      WEXITSTATUS(processStatus) == 0;

  return {
      curlSucceeded && httpStatus >= 200 && httpStatus < 400,
      httpStatus};
}

int logicalProcessorCount() {
  const long processors = sysconf(_SC_NPROCESSORS_ONLN);

  return processors > 0
      ? static_cast<int>(processors)
      : 1;
}

std::string availability(bool available) {
  return available ? "READY" : "NOT AVAILABLE";
}

std::string normalizeBackend(std::string backend) {
  for (char &character : backend) {
    character = static_cast<char>(
        std::tolower(static_cast<unsigned char>(character)));
  }

  return backend;
}

std::string profileStatus(bool backendReady,
                          bool profileReady,
                          bool selected) {
  if (!backendReady) {
    return "N/A";
  }

  if (profileReady) {
    return "READY";
  }

  return selected ? "MISSING" : "OPTIONAL";
}

void printEngine(const std::string &label,
                 const std::optional<std::string> &path) {
  std::cout << label << (path ? "OK" : "MISSING") << '\n';

  if (path) {
    std::cout << "Executable.......... " << *path << '\n';
  }
}

void printDevices(const std::string &backend,
                  const std::vector<GpuInfo> &devices) {
  std::cout << backend << " devices....... " << devices.size() << '\n';

  for (const auto &device : devices) {
    std::cout << "Device.............. " << device.device << ": "
              << device.name << '\n';

    if (device.memoryMb > 0) {
      std::cout << "Memory.............. " << device.memoryMb << " MiB\n";
    }
  }
}

std::string telemetryValue(
    const std::optional<double> &value,
    const std::string &unit) {
  if (!value) {
    return "unavailable";
  }

  std::ostringstream output;
  output << std::fixed << std::setprecision(1) << *value << ' ' << unit;
  return output.str();
}

void printTelemetry(
    const std::vector<GpuTelemetrySnapshot> &snapshots,
    const GpuThermalPolicyConfiguration &policy) {
  std::cout << "\nGPU telemetry\n"
            << "-------------\n";

  if (snapshots.empty()) {
    std::cout << "Readings........... unavailable\n";
    return;
  }

  for (const auto &snapshot : snapshots) {
    std::cout << "Device.............. "
              << snapshot.vendor << ' ' << snapshot.deviceId;

    if (!snapshot.pciBusId.empty()) {
      std::cout << " (" << snapshot.pciBusId << ')';
    }

    std::cout << '\n'
              << "Temperature......... "
              << telemetryValue(snapshot.temperatureC, "C");

    if (snapshot.temperatureC &&
        !snapshot.temperatureLabel.empty()) {
      std::cout << " (" << snapshot.temperatureLabel << ')';
    }

    std::cout << '\n'
              << "Power draw.......... "
              << telemetryValue(snapshot.powerDrawW, "W") << '\n'
              << "Power limit......... "
              << telemetryValue(snapshot.powerLimitW, "W") << '\n'
              << "Thermal state....... "
              << GpuThermalPolicy::stateName(
                     GpuThermalPolicy::evaluate(
                         policy,
                         snapshot.temperatureC))
              << '\n';
  }
}

void printThermalPolicy(
    const GpuThermalPolicyConfiguration &policy) {
  std::cout << "\nThermal policy\n"
            << "--------------\n"
            << "Mode................ "
            << (policy.enabled ? "enabled" : "disabled") << '\n'
            << "Warning threshold... "
            << telemetryValue(policy.warningC, "C") << '\n'
            << "Critical threshold.. "
            << telemetryValue(policy.criticalC, "C") << '\n'
            << "Enforcement......... "
            << (policy.stopOnCritical
                    ? "orderly stop at critical threshold"
                    : "none; diagnostic warnings only")
            << '\n';
}

bool usableProfile(const GpuProfileRecord &profile) {
  return profile.blocks > 0 &&
         profile.threads > 0 &&
         profile.points > 0 &&
         profile.averageSpeed > 0.0;
}

bool hasProfile(const std::vector<GpuProfileRecord> &profiles,
                const std::vector<GpuInfo> &devices,
                const std::string &backend) {
  for (const auto &profile : profiles) {
    if (profile.backend != backend ||
        profile.engine != "BitCrack" ||
        !usableProfile(profile)) {
      continue;
    }

    for (const auto &device : devices) {
      if (profile.gpuName == device.name) {
        return true;
      }
    }
  }

  return false;
}

void warning(DoctorSummary &summary,
             const std::string &code,
             const std::string &problem,
             const std::string &action) {
  ++summary.warnings;
  std::cout << "Warning code....... " << code << '\n'
            << "Problem............ " << problem << '\n'
            << "Action............. " << action << '\n';
}

void failure(DoctorSummary &summary,
             const std::string &code,
             const std::string &problem,
             const std::string &action) {
  ++summary.errors;
  std::cout << "Error code......... " << code << '\n'
            << "Problem............ " << problem << '\n'
            << "Action............. " << action << '\n';
}

} // namespace

int DoctorService::execute(
    const std::vector<std::string> &args) const {
  DoctorSummary summary;

  const auto configuration = ConfigurationManager::load();

  std::string rusticlConfigurationError;
  if (!configuration.gpu.rusticlEnable.empty()) {
    try {
      RusticlEnvironment::apply(configuration.gpu.rusticlEnable);
    } catch (const std::exception &error) {
      rusticlConfigurationError = error.what();
    }
  }

  const auto cudaEngine = ToolManager::bitcrackCudaPath();
  const auto openclEngine = ToolManager::bitcrackOpenCLPath();
  const auto cpuEngine = ToolManager::keyhuntPath();
  const auto cudaDevices = GpuManager::listCudaGpus();
  const auto openclDevices = GpuManager::listOpenClGpus();
  const auto gpuTelemetry = GpuTelemetry::readAll();
  const int processors = logicalProcessorCount();

  const bool cudaReady = cudaEngine.has_value() && !cudaDevices.empty();
  const bool openclReady = openclEngine.has_value() && !openclDevices.empty();
  const bool cpuReady = cpuEngine.has_value() && processors > 0;
  const bool computeReady = cudaReady || openclReady || cpuReady;

  std::string selectedBackend =
      normalizeBackend(configuration.engine.backend);

  if (selectedBackend != "cuda" &&
      selectedBackend != "opencl" &&
      selectedBackend != "cpu") {
    selectedBackend =
        cudaReady
            ? "cuda"
            : (openclReady ? "opencl" : (cpuReady ? "cpu" : "none"));
  }

  const auto configurationPath =
      std::filesystem::path(ToolManager::configPath());
  const auto storagePath = dataPath();
  const auto databasePath = storagePath / "openpuzzle.db";

  std::error_code error;
  const bool configurationPresent =
      std::filesystem::is_regular_file(configurationPath, error);
  error.clear();
  const bool storagePresent =
      std::filesystem::is_directory(storagePath, error);
  const bool storageWritable =
      storagePresent &&
      access(storagePath.c_str(), W_OK | X_OK) == 0;

  std::cout << "OpenPuzzle Doctor\n"
            << "-----------------\n"
#ifdef OPENPUZZLE_VERSION
            << "Version............ " << OPENPUZZLE_VERSION << '\n'
#else
            << "Version............ development\n"
#endif
            << "Configuration...... "
            << (configurationPresent ? "available" : "not created") << '\n'
            << "Configuration path. " << configurationPath.string() << '\n'
            << "Local storage...... "
            << (storagePresent
                    ? (storageWritable ? "writable" : "NOT WRITABLE")
                    : "not created")
            << '\n'
            << "Storage path....... " << storagePath.string() << "\n\n";

  if (!configurationPresent) {
    warning(summary,
            "OP-DOCTOR-002",
            "configuration has not been created yet",
            "run openpuzzle run to complete first-time setup");
  }

  if (!rusticlConfigurationError.empty()) {
    warning(summary,
            "OP-DOCTOR-007",
            "the configured Rusticl selector is invalid",
            "run OpenPuzzle with a valid --rusticl-enable selector");
  }

  if (storagePresent && !storageWritable) {
    failure(summary,
            "OP-DOCTOR-003",
            "local OpenPuzzle storage is not writable",
            "restore write permission for ~/.local/share/OpenPuzzle");
  }

  const bool offline = hasArgument(args, "--offline");
  const std::string serverUrl = coordinationServerUrl();
  const ServerProbeResult serverProbe =
      offline ? ServerProbeResult{} : probeServer(serverUrl);

  std::cout << "\nCoordination server\n"
            << "-------------------\n"
            << "Server URL......... " << serverUrl << '\n'
            << "Server connection.. "
            << (offline
                    ? "SKIPPED"
                    : (serverProbe.reachable ? "REACHABLE" : "UNREACHABLE"))
            << '\n'
            << "Probe.............. HEAD only; no assignment requested\n";

  if (!offline && serverProbe.httpStatus > 0) {
    std::cout << "HTTP status........ " << serverProbe.httpStatus << '\n';
  }

  if (!offline && !serverProbe.reachable) {
    warning(summary,
            "OP-DOCTOR-006",
            "the coordination server could not be reached over HTTPS",
            "check the internet connection or OPENPUZZLE_SERVER_URL");
  }

  std::cout << "\nBundled engines\n"
            << "---------------\n";
  printEngine("CUDA engine........ ", cudaEngine);
  printEngine("OpenCL engine...... ", openclEngine);
  printEngine("CPU engine......... ", cpuEngine);

  std::cout << "\nHardware\n"
            << "--------\n"
            << "Logical processors. " << processors << '\n';
  printDevices("CUDA", cudaDevices);
  printDevices("OpenCL", openclDevices);

  if (!configuration.gpu.rusticlEnable.empty()) {
    std::cout << "Rusticl selector... "
              << configuration.gpu.rusticlEnable << '\n';
  }

  printThermalPolicy(configuration.gpu.thermal);
  printTelemetry(gpuTelemetry, configuration.gpu.thermal);

  if (!GpuThermalPolicy::valid(configuration.gpu.thermal)) {
    warning(summary,
            "OP-DOCTOR-008",
            "the thermal policy thresholds are invalid",
            "set warning to 30-110 C and critical above warning up to 120 C");
  } else if (configuration.gpu.thermal.enabled) {
    for (const auto &snapshot : gpuTelemetry) {
      const auto state = GpuThermalPolicy::evaluate(
          configuration.gpu.thermal,
          snapshot.temperatureC);
      const std::string device = snapshot.vendor + " " + snapshot.deviceId;

      if (state == GpuThermalState::Unavailable) {
        warning(summary,
                "OP-DOCTOR-009",
                device + " has no usable temperature reading",
                "verify the GPU driver and hardware sensor interface");
      } else if (state == GpuThermalState::Warning) {
        warning(summary,
                "OP-DOCTOR-010",
                device + " reached the thermal warning threshold",
                "inspect airflow, fans and heatsink condition");
      } else if (state == GpuThermalState::Critical) {
        warning(summary,
                "OP-DOCTOR-011",
                device + " reached the critical thermal threshold",
                "stop the workload and inspect GPU cooling");
      }
    }
  }

  std::cout << "\nUsable backends\n"
            << "---------------\n"
            << "CUDA backend....... " << availability(cudaReady) << '\n'
            << "OpenCL backend..... " << availability(openclReady) << '\n'
            << "CPU backend........ " << availability(cpuReady) << '\n';

  if (!computeReady) {
    failure(summary,
            "OP-DOCTOR-001",
            "no usable compute backend is available",
            "verify the drivers, then reinstall the OpenPuzzle package");
  }

  std::vector<GpuProfileRecord> profiles;
  error.clear();
  const bool databasePresent =
      std::filesystem::is_regular_file(databasePath, error);

  if (databasePresent) {
    Database database;
    if (database.open(databasePath.string())) {
      profiles = database.listGpuProfiles();
    }
  }

  const bool cudaProfile = hasProfile(profiles, cudaDevices, "CUDA");
  const bool openclProfile = hasProfile(profiles, openclDevices, "OpenCL");

  const bool cudaSelected = selectedBackend == "cuda";
  const bool openclSelected = selectedBackend == "opencl";
  const bool selectedProfileMissing =
      (cudaSelected && cudaReady && !cudaProfile) ||
      (openclSelected && openclReady && !openclProfile);

  std::string profileAction;
  if (cudaSelected) {
    profileAction = "run openpuzzle benchmark --backend cuda";
  } else if (openclSelected) {
    profileAction = "run openpuzzle benchmark --backend opencl";
  }

  std::cout << "\nBenchmark profiles\n"
            << "------------------\n"
            << "Selected backend... " << selectedBackend << '\n'
            << "Stored profiles.... " << profiles.size() << '\n'
            << "CUDA profile....... "
            << profileStatus(cudaReady, cudaProfile, cudaSelected)
            << '\n'
            << "OpenCL profile..... "
            << profileStatus(openclReady, openclProfile, openclSelected)
            << '\n';

  if (selectedProfileMissing) {
    warning(summary,
            "OP-DOCTOR-005",
            "the selected GPU backend has no valid benchmark profile",
            profileAction);
  }

  const std::vector<std::string> slots = {
      "primary", "gpu", "cpu", "cuda", "opencl"};
  int activeRuntimes = 0;
  int stalePidFiles = 0;
  int preservedStates = 0;

  for (const auto &slot : slots) {
    const auto pid = ClientRuntimeControl::runtimePid(slot);
    const bool running = ClientRuntimeControl::running(slot);
    const bool pidFilePresent =
        std::filesystem::is_regular_file(
            ClientRuntimeControl::pidPath(slot), error);
    const bool statePresent =
        client::ClientStateStore::load(slot).has_value();

    if (running) {
      ++activeRuntimes;
    } else if (pidFilePresent && pid.has_value()) {
      ++stalePidFiles;
    }

    if (statePresent && !running) {
      ++preservedStates;
    }
  }

  std::cout << "\nRuntime state\n"
            << "-------------\n"
            << "Active runtimes.... " << activeRuntimes << '\n'
            << "Preserved states... " << preservedStates << '\n'
            << "Stale PID files.... " << stalePidFiles << '\n';

  if (stalePidFiles > 0) {
    warning(summary,
            "OP-DOCTOR-004",
            "stale runtime PID control files were found",
            "run openpuzzle run; stale controls are cleaned safely on startup");
  }

  const bool ready = computeReady && summary.errors == 0;
  const std::string status =
      !ready
          ? "NOT READY"
          : (summary.warnings > 0 ? "READY WITH WARNINGS" : "READY");

  std::cout << "\nSummary\n"
            << "-------\n"
            << "Warnings........... " << summary.warnings << '\n'
            << "Errors............. " << summary.errors << '\n'
            << "Status............. " << status << '\n';

  return ready ? 0 : 1;
}

} // namespace openpuzzle

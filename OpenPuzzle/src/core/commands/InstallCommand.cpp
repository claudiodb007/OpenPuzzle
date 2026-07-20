#include "openpuzzle/core/commands/InstallCommand.hpp"

#include "openpuzzle/config/ConfigurationManager.hpp"
#include "openpuzzle/core/CommandContext.hpp"
#include "openpuzzle/core/commands/BenchmarkCommand.hpp"
#include "openpuzzle/hardware/GpuManager.hpp"
#include "openpuzzle/performance/GpuProfileManager.hpp"
#include "openpuzzle/runtime/ClientRuntimeControl.hpp"
#include "openpuzzle/setup/FirstRunSetup.hpp"
#include "openpuzzle/tools/ToolManager.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace openpuzzle {

namespace {

bool hasOnlySupportedArguments(
    const std::vector<std::string> &args) {
  for (const auto &argument : args) {
    if (argument != "--force") {
      return false;
    }
  }

  return true;
}

std::string backendLabel(
    const std::string &backend) {
  return backend == "opencl"
             ? "OpenCL"
             : "CUDA";
}

void printReady(
    const std::string &backend,
    const GpuInfo &gpu,
    bool reusedProfile) {
  std::cout
      << "\nOpenPuzzle ready\n"
      << "----------------\n"
      << "Backend............ "
      << backendLabel(backend)
      << '\n'
      << "GPU................ "
      << gpu.name
      << '\n'
      << "GPU profile........ "
      << (reusedProfile
              ? "existing profile reused"
              : "automatic benchmark completed")
      << '\n'
      << "Next command........ openpuzzle run\n";
}

} // namespace

InstallCommand::InstallCommand()
    : InstallCommand(
          productionDependencies()) {}

InstallCommand::InstallCommand(
    InstallCommandDependencies dependencies)
    : dependencies_(
          std::move(dependencies)) {
  if (!dependencies_.runtimeRunning ||
      !dependencies_.ensureConfigured ||
      !dependencies_.configuredBackend ||
      !dependencies_.currentGpu ||
      !dependencies_.hasValidProfile ||
      !dependencies_.benchmark) {
    throw std::invalid_argument(
        "Incomplete InstallCommand dependencies");
  }
}

int InstallCommand::run(
    const std::vector<std::string> &args) const {
  if (!hasOnlySupportedArguments(args)) {
    std::cerr
        << "Usage: openpuzzle install [--force]\n";

    return 1;
  }

  if (dependencies_.runtimeRunning()) {
    std::cerr
        << "OpenPuzzle is currently running.\n"
        << "Stop it before changing the local setup.\n";

    return 1;
  }

  std::cout
      << "OpenPuzzle installation\n"
      << "-----------------------\n";

  if (!dependencies_.ensureConfigured()) {
    return 1;
  }

  const std::string backend =
      dependencies_.configuredBackend();

  if (!ToolManager::supportsBackend(backend)) {
    std::cerr
        << "No supported GPU backend was detected.\n";

    return 1;
  }

  const std::string label =
      backendLabel(backend);

  const GpuInfo gpu =
      dependencies_.currentGpu(label);

  const bool force =
      !args.empty() &&
      args.front() == "--force";

  const bool existingProfile =
      dependencies_.hasValidProfile(
          gpu.name,
          label);

  if (existingProfile && !force) {
    printReady(
        backend,
        gpu,
        true);

    return 0;
  }

  std::cout
      << "Backend............ "
      << label
      << '\n'
      << "GPU................ "
      << gpu.name
      << '\n'
      << "GPU profile........ "
      << (force
              ? "benchmark requested"
              : "not found")
      << '\n'
      << "Benchmark.......... starting\n"
      << "Server contact..... none\n\n";

  const int benchmarkResult =
      dependencies_.benchmark({
          "--real",
          "--auto",
          "--backend",
          backend,
          "--gpu",
          std::to_string(gpu.device),
      });

  if (benchmarkResult != 0) {
    std::cerr
        << "Automatic benchmark failed.\n";

    return benchmarkResult;
  }

  if (!dependencies_.hasValidProfile(
          gpu.name,
          label)) {
    std::cerr
        << "Benchmark completed without a valid GPU profile.\n";

    return 1;
  }

  printReady(
      backend,
      gpu,
      false);

  return 0;
}

InstallCommandDependencies
InstallCommand::productionDependencies() {
  InstallCommandDependencies dependencies;

  dependencies.runtimeRunning = [] {
    return ClientRuntimeControl::running();
  };

  dependencies.ensureConfigured = [] {
    FirstRunSetup setup;
    return setup.ensureConfigured();
  };

  dependencies.configuredBackend = [] {
    const auto configuration =
        ConfigurationManager::load();

    if (!configuration.engine.backend.empty()) {
      return configuration.engine.backend;
    }

    return ToolManager::preferredBackend();
  };

  dependencies.currentGpu =
      [](const std::string &label) {
        return GpuManager::currentGpu(label);
      };

  dependencies.hasValidProfile =
      [](const std::string &gpuName,
         const std::string &label) {
        CommandContext context;

        if (!context.initialize()) {
          return false;
        }

        GpuProfileManager profiles(
            context.db);

        const auto profile =
            profiles.chooseBest(
                gpuName,
                label,
                "BitCrack");

        return
            profile &&
            profile->blocks > 0 &&
            profile->threads > 0 &&
            profile->points > 0 &&
            profile->averageSpeed > 0.0;
      };

  dependencies.benchmark =
      [](const std::vector<std::string> &args) {
        return BenchmarkCommand().run(args);
      };

  return dependencies;
}

} // namespace openpuzzle

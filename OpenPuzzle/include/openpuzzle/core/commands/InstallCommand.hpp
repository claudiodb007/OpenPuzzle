#pragma once

#include "openpuzzle/hardware/GpuInfo.hpp"

#include <functional>
#include <string>
#include <vector>

namespace openpuzzle {

struct InstallCommandDependencies {
  std::function<bool()> runtimeRunning;
  std::function<bool()> ensureConfigured;
  std::function<std::string()> configuredBackend;

  std::function<GpuInfo(
      const std::string &backendLabel)>
      currentGpu;

  std::function<bool(
      const std::string &gpuName,
      const std::string &backendLabel)>
      hasValidProfile;

  std::function<int(
      const std::vector<std::string> &args)>
      benchmark;
};

class InstallCommand {
public:
  InstallCommand();

  explicit InstallCommand(
      InstallCommandDependencies dependencies);

  int run(
      const std::vector<std::string> &args) const;

private:
  InstallCommandDependencies dependencies_;

  static InstallCommandDependencies
  productionDependencies();
};

} // namespace openpuzzle

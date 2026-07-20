#include "openpuzzle/core/commands/InstallCommand.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

using namespace openpuzzle;

namespace {

struct State {
  bool running = false;
  bool configured = true;
  bool profile = true;
  bool profileAfterBenchmark = true;
  int benchmarkResult = 0;

  int configureCalls = 0;
  int profileCalls = 0;
  int benchmarkCalls = 0;

  std::vector<std::string> benchmarkArgs;
};

InstallCommand makeCommand(
    State &state) {
  InstallCommandDependencies dependencies;

  dependencies.runtimeRunning =
      [&state] {
        return state.running;
      };

  dependencies.ensureConfigured =
      [&state] {
        ++state.configureCalls;
        return state.configured;
      };

  dependencies.configuredBackend = [] {
    return std::string{"opencl"};
  };

  dependencies.currentGpu =
      [](const std::string &label) {
        assert(label == "OpenCL");

        GpuInfo gpu;
        gpu.device = 3;
        gpu.name = "Test GPU";
        gpu.backend = label;
        gpu.opencl = true;
        return gpu;
      };

  dependencies.hasValidProfile =
      [&state](const std::string &gpuName,
               const std::string &label) {
        assert(gpuName == "Test GPU");
        assert(label == "OpenCL");

        ++state.profileCalls;

        if (state.benchmarkCalls > 0) {
          return state.profileAfterBenchmark;
        }

        return state.profile;
      };

  dependencies.benchmark =
      [&state](
          const std::vector<std::string> &args) {
        ++state.benchmarkCalls;
        state.benchmarkArgs = args;
        return state.benchmarkResult;
      };

  return InstallCommand(
      std::move(dependencies));
}

} // namespace

int main() {
  {
    State state;
    const auto command = makeCommand(state);

    assert(command.run({}) == 0);
    assert(state.configureCalls == 1);
    assert(state.profileCalls == 1);
    assert(state.benchmarkCalls == 0);
  }

  {
    State state;
    state.profile = false;

    const auto command = makeCommand(state);

    assert(command.run({}) == 0);
    assert(state.profileCalls == 2);
    assert(state.benchmarkCalls == 1);

    const std::vector<std::string> expected = {
        "--real",
        "--auto",
        "--backend",
        "opencl",
        "--gpu",
        "3",
    };

    assert(state.benchmarkArgs == expected);
  }

  {
    State state;
    const auto command = makeCommand(state);

    assert(command.run({"--force"}) == 0);
    assert(state.benchmarkCalls == 1);
    assert(state.profileCalls == 2);
  }

  {
    State state;
    state.running = true;

    const auto command = makeCommand(state);

    assert(command.run({}) == 1);
    assert(state.configureCalls == 0);
    assert(state.benchmarkCalls == 0);
  }

  {
    State state;
    state.configured = false;

    const auto command = makeCommand(state);

    assert(command.run({}) == 1);
    assert(state.benchmarkCalls == 0);
  }

  {
    State state;
    state.profile = false;
    state.benchmarkResult = 7;

    const auto command = makeCommand(state);

    assert(command.run({}) == 7);
    assert(state.benchmarkCalls == 1);
  }

  {
    State state;
    state.profile = false;
    state.profileAfterBenchmark = false;

    const auto command = makeCommand(state);

    assert(command.run({}) == 1);
    assert(state.benchmarkCalls == 1);
  }

  {
    State state;
    const auto command = makeCommand(state);

    assert(command.run({"--unknown"}) == 1);
    assert(state.configureCalls == 0);
    assert(state.benchmarkCalls == 0);
  }

  std::cout
      << "InstallCommandTests passed\n";

  return 0;
}

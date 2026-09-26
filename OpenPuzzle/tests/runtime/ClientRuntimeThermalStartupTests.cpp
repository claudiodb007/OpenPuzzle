#include "openpuzzle/runtime/ClientRuntime.hpp"

#include <cassert>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>

using namespace openpuzzle;

namespace {

ClientRuntimeDependencies dependencies(
    bool &stopRequested,
    int &thermalPolls,
    int &releaseCalls) {
  ClientRuntimeDependencies result;

  result.sync = [](const std::string &) {
    return client::ExecutionSyncResult{};
  };
  result.heartbeat = [](const std::string &) {
    client::ClientHeartbeatResult heartbeat;
    heartbeat.success = true;
    return heartbeat;
  };
  result.stopExecution = [](const std::string &) {
    return true;
  };
  result.reportSolution = [](
      const std::string &,
      const std::string &,
      const std::string &,
      std::string &) {
    return true;
  };
  result.finalizeAssignment = [](
      const std::string &,
      const std::string &,
      const std::string &,
      int,
      const std::string &,
      const std::string &,
      std::string &) {
    return client::AssignmentUploadStatus::Uploaded;
  };
  result.finalKeysChecked = [](const std::string &) {
    return std::string{"0"};
  };
  result.removeState = [] {
    return true;
  };
  result.acquireRuntime = [] {
    return true;
  };
  result.releaseRuntime = [&] {
    ++releaseCalls;
  };
  result.clearSafeStop = [] {
    return true;
  };
  result.prepareSignals = [] {};
  result.stopRequested = [&] {
    return stopRequested;
  };
  result.safeStopRequested = [] {
    return false;
  };
  result.hasState = [] {
    return false;
  };
  result.sleep = [](std::chrono::seconds) {};
  result.thermalPoll = [&] {
    ++thermalPolls;
    return false;
  };

  return result;
}

} // namespace

int main() {
  /* Critical protection requests stop during the startup sample. */
  {
    bool stopRequested = false;
    int thermalPolls = 0;
    int releaseCalls = 0;
    int assignments = 0;

    auto configured = dependencies(
        stopRequested,
        thermalPolls,
        releaseCalls);
    configured.thermalPoll = [&] {
      ++thermalPolls;
      stopRequested = true;
      return true;
    };

    ClientRuntime runtime(std::move(configured));

    std::ostringstream standardOutput;
    std::ostringstream errorOutput;
    auto *oldStandard = std::cout.rdbuf(standardOutput.rdbuf());
    auto *oldError = std::cerr.rdbuf(errorOutput.rdbuf());

    const int result = runtime.runContinuous(
        "https://server.test",
        [&] {
          ++assignments;
          return ClientIterationResult::solutionFound();
        });

    std::cout.rdbuf(oldStandard);
    std::cerr.rdbuf(oldError);

    assert(result == 0);
    assert(thermalPolls == 1);
    assert(assignments == 0);
    assert(releaseCalls == 1);
    assert(errorOutput.str().find(
               "Startup............. blocked") !=
           std::string::npos);
    assert(errorOutput.str().find(
               "Assignment......... not requested") !=
           std::string::npos);
  }

  /* Diagnostic sampling does not block the existing assignment path. */
  {
    bool stopRequested = false;
    int thermalPolls = 0;
    int releaseCalls = 0;
    int assignments = 0;

    ClientRuntime runtime(
        dependencies(
            stopRequested,
            thermalPolls,
            releaseCalls));

    const int result = runtime.runContinuous(
        "https://server.test",
        [&] {
          ++assignments;
          return ClientIterationResult::solutionFound();
        });

    assert(result == 0);
    assert(thermalPolls == 1);
    assert(assignments == 1);
    assert(releaseCalls == 1);
  }

  return 0;
}

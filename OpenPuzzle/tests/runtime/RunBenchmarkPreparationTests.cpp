#include "openpuzzle/runtime/RunBenchmarkPreparation.hpp"

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

using namespace openpuzzle;

namespace {

struct State {
  bool profile = true;
  bool profileAfterBenchmark = true;
  int benchmarkResult = 0;
  int profileChecks = 0;
  int benchmarkCalls = 0;
};

struct CapturedPreparation {
  bool success = false;
  std::string output;
};

CapturedPreparation runCaptured(
    const RunBenchmarkPreparation &preparation) {
  std::ostringstream output;
  auto *oldOutput = std::cout.rdbuf(output.rdbuf());
  auto *oldError = std::cerr.rdbuf(output.rdbuf());

  const bool success =
      preparation.ensureProfile();

  std::cout.rdbuf(oldOutput);
  std::cerr.rdbuf(oldError);

  return {success, output.str()};
}

RunBenchmarkPreparation makePreparation(
    State &state) {
  RunBenchmarkPreparationDependencies dependencies;

  dependencies.hasValidProfile =
      [&state] {
        ++state.profileChecks;

        if (state.benchmarkCalls > 0) {
          return state.profileAfterBenchmark;
        }

        return state.profile;
      };

  dependencies.runBenchmark =
      [&state] {
        ++state.benchmarkCalls;
        return state.benchmarkResult;
      };

  return RunBenchmarkPreparation(
      std::move(dependencies));
}

} // namespace

int main() {
  {
    State state;
    const auto preparation = makePreparation(state);

    assert(preparation.ensureProfile());
    assert(state.profileChecks == 1);
    assert(state.benchmarkCalls == 0);
  }

  {
    State state;
    state.profile = false;

    const auto preparation = makePreparation(state);

    const auto captured =
        runCaptured(preparation);

    assert(captured.success);
    assert(state.profileChecks == 2);
    assert(state.benchmarkCalls == 1);
    assert(captured.output.find("[1/4] Hardware and engine") != std::string::npos);
    assert(captured.output.find("[3/4] Safe benchmark....... complete") != std::string::npos);
    assert(captured.output.find("[4/4] Server contact....... ready") != std::string::npos);
    assert(captured.output.find("OpenPuzzle will now request work") != std::string::npos);
  }

  {
    State state;
    state.profile = false;
    state.benchmarkResult = 7;

    const auto preparation = makePreparation(state);

    const auto captured =
        runCaptured(preparation);

    assert(!captured.success);
    assert(captured.output.find("OP-BENCH-001") != std::string::npos);
    assert(captured.output.find("openpuzzle doctor") != std::string::npos);
    assert(captured.output.find("openpuzzle benchmark --real --auto") != std::string::npos);
    assert(captured.output.find("Server contact not started") != std::string::npos);
    assert(state.profileChecks == 1);
    assert(state.benchmarkCalls == 1);
  }

  {
    State state;
    state.profile = false;
    state.profileAfterBenchmark = false;

    const auto preparation = makePreparation(state);

    const auto captured =
        runCaptured(preparation);

    assert(!captured.success);
    assert(captured.output.find("OP-BENCH-002") != std::string::npos);
    assert(captured.output.find("openpuzzle doctor") != std::string::npos);
    assert(state.profileChecks == 2);
    assert(state.benchmarkCalls == 1);
  }

  std::cout
      << "RunBenchmarkPreparationTests passed\n";

  return 0;
}

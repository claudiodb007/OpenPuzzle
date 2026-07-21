#include "openpuzzle/runtime/RunBenchmarkPreparation.hpp"

#include <cassert>
#include <iostream>
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

    assert(preparation.ensureProfile());
    assert(state.profileChecks == 2);
    assert(state.benchmarkCalls == 1);
  }

  {
    State state;
    state.profile = false;
    state.benchmarkResult = 7;

    const auto preparation = makePreparation(state);

    assert(!preparation.ensureProfile());
    assert(state.profileChecks == 1);
    assert(state.benchmarkCalls == 1);
  }

  {
    State state;
    state.profile = false;
    state.profileAfterBenchmark = false;

    const auto preparation = makePreparation(state);

    assert(!preparation.ensureProfile());
    assert(state.profileChecks == 2);
    assert(state.benchmarkCalls == 1);
  }

  std::cout
      << "RunBenchmarkPreparationTests passed\n";

  return 0;
}

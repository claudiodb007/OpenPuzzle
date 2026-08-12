#include "openpuzzle/core/Application.hpp"

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace openpuzzle;

namespace {

struct CapturedRun {
  int exitCode = 1;
  std::string output;
};

CapturedRun runApplication(
    std::vector<std::string> arguments) {
  std::vector<char *> argv;

  argv.reserve(
      arguments.size());

  for (auto &argument : arguments) {
    argv.push_back(
        argument.data());
  }

  std::ostringstream output;

  auto *oldOutput =
      std::cout.rdbuf(
          output.rdbuf());

  auto *oldError =
      std::cerr.rdbuf(
          output.rdbuf());

  Application application;

  const int exitCode =
      application.run(
          static_cast<int>(
              argv.size()),
          argv.data());

  std::cout.rdbuf(oldOutput);
  std::cerr.rdbuf(oldError);

  return {
      exitCode,
      output.str()
  };
}

void assertNoExecution(
    const std::string &output) {
  assert(
      output.find(
          "Requesting assignment") ==
      std::string::npos);

  assert(
      output.find(
          "Requested puzzle") ==
      std::string::npos);

  assert(
      output.find(
          "BitCrack started") ==
      std::string::npos);
}

} // namespace

int main() {
  /*
   * --dry-run makes the regression case local
   * even if informational routing is broken.
   */
  {
    const auto result =
        runApplication({
            "OpenPuzzle",
            "--help",
            "--dry-run"
        });

    assert(result.exitCode == 0);

    assert(
        result.output.find(
            "Usage:") !=
        std::string::npos);

    assert(
        result.output.find(
            "--version") !=
        std::string::npos);

    assert(
        result.output.find(
            "openpuzzle safestop") !=
        std::string::npos);

    assert(
        result.output.find(
            "openpuzzle update") !=
        std::string::npos);

    assert(
        result.output.find(
            "--safe") !=
        std::string::npos);

    assert(
        result.output.find(
            "openpuzzle selftest") !=
        std::string::npos);

    assert(
        result.output.find(
            "--rusticl-enable") !=
        std::string::npos);

    assertNoExecution(
        result.output);
  }

  {
    const auto result =
        runApplication({
            "OpenPuzzle",
            "help",
            "--dry-run"
        });

    assert(result.exitCode == 0);

    assert(
        result.output.find(
            "Usage:") !=
        std::string::npos);

    assertNoExecution(
        result.output);
  }

  {
    const auto result =
        runApplication({
            "OpenPuzzle",
            "--version",
            "--dry-run"
        });

    assert(result.exitCode == 0);

#ifdef OPENPUZZLE_VERSION
    assert(
        result.output ==
        std::string{
            "OpenPuzzle "
        } +
        OPENPUZZLE_VERSION +
        "\n");
#else
    assert(
        result.output ==
        "OpenPuzzle development\n");
#endif

    assertNoExecution(
        result.output);
  }

  {
    const auto result =
        runApplication({
            "OpenPuzzle",
            "version",
            "--dry-run"
        });

    assert(result.exitCode == 0);

    assert(
        result.output.find(
            "OpenPuzzle ") ==
        0);

    assertNoExecution(
        result.output);
  }

  {
    const auto result =
        runApplication({
            "OpenPuzzle",
            "selftest"
        });

    assert(result.exitCode != 0);
    assert(
        result.output.find(
            "selftest requires --backend") !=
        std::string::npos);
    assertNoExecution(result.output);
  }

  {
    const auto result =
        runApplication({
            "OpenPuzzle",
            "selftest",
            "--backend",
            "all"
        });

    assert(result.exitCode != 0);
    assert(
        result.output.find(
            "Unsupported selftest backend") !=
        std::string::npos);
    assertNoExecution(result.output);
  }

  {
    /*
     * Public solved puzzle-20 regression test. The CPU
     * selftest must accept KeyHunt's successful hit from
     * its protected engine log.
     */
    const auto result =
        runApplication({
            "OpenPuzzle",
            "selftest",
            "--backend",
            "cpu",
            "--threads",
            "1"
        });

    assert(result.exitCode == 0);
    assert(
        result.output.find(
            "Backend............ cpu") !=
        std::string::npos);
    assert(
        result.output.find(
            "Puzzle............. 20") !=
        std::string::npos);
    assert(
        result.output.find(
            "Result............. passed") !=
        std::string::npos);
    assert(
        result.output.find(
            "Diagnostics.........") ==
        std::string::npos);
  }

  {
    const auto result =
        runApplication({
            "OpenPuzzle",
            "benchmark",
            "--backend",
            "invalid"
        });

    assert(result.exitCode != 0);
    assert(result.output.find("OP-BENCH-003") != std::string::npos);
    assert(result.output.find("Action.............") != std::string::npos);
    assertNoExecution(result.output);
  }

  {
    const auto result =
        runApplication({
            "OpenPuzzle",
            "doctor"
        });

    assert(
        result.exitCode == 0 ||
        result.exitCode == 1);

    assert(
        result.output.find(
            "OpenPuzzle Doctor") !=
        std::string::npos);

    assert(
        result.output.find(
            "CUDA backend") !=
        std::string::npos);

    assert(
        result.output.find(
            "OpenCL backend") !=
        std::string::npos);

    assert(
        result.output.find(
            "CPU backend") !=
        std::string::npos);

    if (result.exitCode == 1) {
      assert(
          result.output.find("OP-DOCTOR-001") !=
          std::string::npos);
    }

    assertNoExecution(
        result.output);
  }

  std::cout
      << "ApplicationCliTests passed\n";

  return 0;
}

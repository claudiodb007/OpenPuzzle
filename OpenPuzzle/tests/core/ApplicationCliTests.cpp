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

  std::cout
      << "ApplicationCliTests passed\n";

  return 0;
}

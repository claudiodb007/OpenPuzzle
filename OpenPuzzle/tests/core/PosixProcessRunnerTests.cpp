#include "openpuzzle/core/PosixProcessRunner.hpp"

#include <string>
#include <vector>

using namespace openpuzzle;

int main() {
  PosixProcessRunner runner;

  /*
   * Conventional newline-delimited output.
   */
  {
    std::string output;

    const auto result =
        runner.run(
            "printf 'hello\\n'",
            [&](const std::string &line) {
              output = line;
            });

    if (!result.started)
      return 1;

    if (result.exitCode != 0)
      return 2;

    if (output != "hello")
      return 3;
  }

  /*
   * GPU engines commonly update one terminal line
   * using carriage returns instead of newlines.
   */
  {
    std::vector<std::string> lines;

    const auto result =
        runner.run(
            "printf 'first\\rsecond\\rthird\\n'",
            [&](const std::string &line) {
              lines.push_back(line);
            });

    if (!result.started)
      return 4;

    if (result.exitCode != 0)
      return 5;

    if (lines.size() != 3)
      return 6;

    if (lines[0] != "first")
      return 7;

    if (lines[1] != "second")
      return 8;

    if (lines[2] != "third")
      return 9;
  }

  return 0;
}

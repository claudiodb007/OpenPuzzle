#include "openpuzzle/core/CliInteger.hpp"

#include <cassert>
#include <limits>
#include <string>
#include <vector>

using namespace openpuzzle;

int main() {
  assert(cliIntegerArgument({}, {"--number"}, 9) == 9);
  for (const int value : {0, 1, -1, std::numeric_limits<int>::min(),
                          std::numeric_limits<int>::max()}) {
    assert(cliIntegerArgument({"--number", std::to_string(value)},
                              {"--number"}, 9) == value);
  }
  assert(cliIntegerArgument({"--number", "+12"}, {"--number"}, 9) == 12);
  assert(cliIntegerArgument({"--n", "0012"}, {"--number", "--n"}, 9) == 12);
  for (const auto& args : std::vector<std::vector<std::string>>{
           {"--number"}, {"--number", ""}, {"--number", "--other"},
           {"--number", "1.5"}, {"--number", "1text"}, {"--number", "1e3"},
           {"--number", " 1"}, {"--number", "1 "}, {"--number", "+"},
           {"--number", "+-1"}, {"--number", "2147483648"},
           {"--number", "-2147483649"}, {"--number", std::string(100, '9')},
           {"--number", "1", "--number", "1"},
           {"--number", "1", "--n", "2"}}) {
    bool rejected = false;
    try { (void) cliIntegerArgument(args, {"--number", "--n"}, 9); }
    catch (const std::exception& error) {
      rejected = std::string(error.what()).find("--") != std::string::npos;
    }
    assert(rejected);
  }
}

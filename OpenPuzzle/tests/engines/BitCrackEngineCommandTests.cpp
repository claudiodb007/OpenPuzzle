#include "openpuzzle/engines/bitcrack/BitCrackEngine.hpp"

#include <iostream>
#include <string>

using namespace openpuzzle;

static int fail(const std::string& message) {
  std::cerr << message << "\n";
  return 1;
}

int main() {
  BitCrackEngine engine("cuBitCrack");

  EngineLaunchRequest request;

  request.puzzle.address = "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU";
  request.range.startKey = "400000000000000000";
  request.range.endKey = "7FFFFFFFFFFFFFFFFF";

  request.device = 0;
  request.blocks = 256;
  request.threads = 256;
  request.points = 1024;

  request.outputFile = "/tmp/openpuzzle/found.txt";
  request.logFile = "/tmp/openpuzzle/bitcrack.log";

  const auto command = engine.buildCommand(request);

  const std::string expected =
      "cuBitCrack "
      "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU "
      "--keyspace "
      "400000000000000000:7FFFFFFFFFFFFFFFFF "
      "--out "
      "/tmp/openpuzzle/found.txt "
      "-d 0 "
      "-b 256 "
      "-t 256 "
      "-p 1024 "
      "2>&1 | tee -a "
      "/tmp/openpuzzle/bitcrack.log";

  if (command != expected) {
    std::cerr << "Expected:\n" << expected << "\n\n";
    std::cerr << "Actual:\n" << command << "\n";
    return 1;
  }

  return 0;
}

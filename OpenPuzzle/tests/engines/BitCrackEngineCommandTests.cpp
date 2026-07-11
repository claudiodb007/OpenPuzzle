#include "openpuzzle/engines/EngineLaunchRequest.hpp"
#include "openpuzzle/engines/bitcrack/BitCrackEngine.hpp"

#include <cassert>
#include <iostream>
#include <string>

using namespace openpuzzle;

static bool contains(
    const std::string& text,
    const std::string& part) {
  return text.find(part) != std::string::npos;
}

int main() {
  BitCrackEngine engine(
      "/tmp/cuBitCrack");

  EngineLaunchRequest request;

  request.engine = "BitCrack";
  request.backend = "CUDA";

  request.targets.push_back(
      "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU");

  request.startKey =
      "400000000000000000";

  request.endKey =
      "40000000FFFFFFFFFF";

  request.device = 1;
  request.blocks = 256;
  request.threads = 512;
  request.points = 2048;

  request.outputFile =
      "/tmp/found.txt";

  request.logFile =
      "/tmp/bitcrack.log";

  const auto command =
      engine.buildCommand(request);

  assert(contains(
      command,
      "/tmp/cuBitCrack"));

  assert(contains(
      command,
      "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU"));

  assert(contains(
      command,
      "--keyspace "
      "400000000000000000:"
      "40000000FFFFFFFFFF"));

  assert(contains(
      command,
      "-d 1"));

  assert(contains(
      command,
      "-b 256"));

  assert(contains(
      command,
      "-t 512"));

  assert(contains(
      command,
      "-p 2048"));

  assert(contains(
      command,
      "--out /tmp/found.txt"));

  assert(contains(
      command,
      "tee -a /tmp/bitcrack.log"));

  std::cout
      << "BitCrackEngineCommandTests passed\n";

  return 0;
}

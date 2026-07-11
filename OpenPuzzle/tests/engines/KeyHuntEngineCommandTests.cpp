#include "openpuzzle/engines/EngineLaunchRequest.hpp"
#include "openpuzzle/engines/keyhunt/KeyHuntEngine.hpp"

#include <cassert>
#include <iostream>
#include <string>

using namespace openpuzzle;

static bool contains(
    const std::string& text,
    const std::string& part) {
  return text.find(part) !=
         std::string::npos;
}

int main() {
  KeyHuntEngine engine(
      "/tmp/keyhunt");

  EngineLaunchRequest request;

  request.puzzle.address =
      "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU";

  request.range.startKey =
      "400000000000000000";

  request.range.endKey =
      "40000000FFFFFFFFFF";

  request.targetFile =
      "/tmp/openpuzzle-targets.txt";

  request.logFile =
      "/tmp/keyhunt.log";

  request.threads = 16;

  auto command =
      engine.buildCommand(request);

  assert(contains(
      command,
      "/tmp/keyhunt"));

  assert(contains(
      command,
      "-m address"));

  assert(contains(
      command,
      "-f /tmp/openpuzzle-targets.txt"));

  assert(contains(
      command,
      "-r 400000000000000000:"
      "40000000FFFFFFFFFF"));

  assert(contains(
      command,
      "-t 16"));

  assert(contains(
      command,
      "tee -a /tmp/keyhunt.log"));

  assert(!contains(
      command,
      request.puzzle.address));

  std::cout
      << "KeyHuntEngineCommandTests passed\n";

  return 0;
}

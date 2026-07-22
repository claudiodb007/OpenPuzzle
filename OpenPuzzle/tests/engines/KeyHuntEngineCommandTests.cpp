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

  request.targets.push_back(
      "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU");

  request.startKey =
      "400000000000000000";

  request.endKey =
      "40000000FFFFFFFFFF";

  request.targetFile =
      "/tmp/openpuzzle-targets.txt";

  request.workspace =
      "/tmp/openpuzzle-workspace";

  request.logFile =
      "/tmp/keyhunt.log";

  request.threads = 16;

  auto command =
      engine.buildCommand(request);

  assert(contains(
      command,
      "cd '/tmp/openpuzzle-workspace'"));

  assert(contains(
      command,
      "umask 077"));

  assert(contains(
      command,
      ": > KEYFOUNDKEYFOUND.txt"));

  assert(contains(
      command,
      "chmod 600 KEYFOUNDKEYFOUND.txt"));

  assert(contains(
      command,
      "ln -sfn KEYFOUNDKEYFOUND.txt found.txt"));

  assert(contains(
      command,
      "'/tmp/keyhunt'"));

  assert(contains(
      command,
      "-m address"));

  assert(contains(
      command,
      "-f '/tmp/openpuzzle-targets.txt'"));

  assert(contains(
      command,
      "-r '400000000000000000:"
      "40000000FFFFFFFFFF'"));

  assert(contains(
      command,
      "-l compress"));

  assert(contains(
      command,
      "-n 1024"));

  assert(contains(
      command,
      "-q"));

  assert(contains(
      command,
      "-s 1"));

  assert(contains(
      command,
      "-t 16"));

  assert(contains(
      command,
      ">> '/tmp/keyhunt.log' 2>&1"));

  assert(!contains(
      command,
      "tee"));

  assert(!contains(
      command,
      request.targets.front()));

  std::cout
      << "KeyHuntEngineCommandTests passed\n";

  return 0;
}

#include "openpuzzle/engines/EngineLaunchRequest.hpp"
#include "openpuzzle/engines/kangaroo/KangarooEngine.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace openpuzzle;

namespace {

bool contains(const std::string &text, const std::string &part) {
  return text.find(part) != std::string::npos;
}

bool rejected(KangarooEngine &engine, EngineLaunchRequest request) {
  try {
    (void)engine.buildCommand(request);
    return false;
  } catch (const std::invalid_argument &) {
    return true;
  }
}

} // namespace

int main() {
  KangarooEngine engine("/tmp/psckangaroo");

  EngineLaunchRequest request;
  request.engine = "Kangaroo";
  request.backend = "CUDA";
  request.publicKey =
      "031f6a332d3c5c4f2de2378c012f429cd109ba07d69690c6c701b6bb87860d6640";
  request.startKey = "100000000";
  request.endKey = "1FFFFFFFF";
  request.device = 1;
  request.workspace = "/tmp/openpuzzle-kangaroo";
  request.outputFile = "/tmp/openpuzzle-kangaroo/found.txt";
  request.logFile = "/tmp/openpuzzle-kangaroo/kangaroo.log";

  const auto command = engine.buildCommand(request);

  assert(engine.info().name == "Pollard Kangaroo");
  assert(engine.info().backend == "CUDA");
  assert(contains(command, "cd '/tmp/openpuzzle-kangaroo'"));
  assert(contains(command, "umask 077"));
  assert(contains(command, ": > '/tmp/openpuzzle-kangaroo/found.txt'"));
  assert(contains(command, "chmod 600 '/tmp/openpuzzle-kangaroo/found.txt'"));
  assert(contains(command, ": > '/tmp/openpuzzle-kangaroo/RESULTS.TXT'"));
  assert(contains(command, "chmod 600 '/tmp/openpuzzle-kangaroo/RESULTS.TXT'"));
  assert(contains(command, "&& { '/tmp/psckangaroo'"));
  assert(contains(command, "status=$?"));
  assert(contains(command, "if [ -s '/tmp/openpuzzle-kangaroo/RESULTS.TXT' ]"));
  assert(contains(command, "cp -- '/tmp/openpuzzle-kangaroo/RESULTS.TXT' '/tmp/openpuzzle-kangaroo/found.txt'"));
  assert(contains(command, "exit $status"));
  assert(contains(command, "-gpu 1"));
  assert(contains(command, "-dp 16"));
  assert(contains(command, "-range 32"));
  assert(contains(command, "-pubkey '" + request.publicKey + "'"));
  assert(contains(command, "-start '100000000'"));
  assert(contains(command, "-ramlimit 8"));
  assert(contains(command, "-concurrent 1"));
  assert(contains(command, "-wwbuffer 5"));
  assert(contains(command, "-checkpoint 0"));
  assert(contains(command, ">> '/tmp/openpuzzle-kangaroo/kangaroo.log' 2>&1"));
  assert(!contains(command, request.endKey));
  assert(!contains(command, "tee"));

  auto invalidWidth = request;
  invalidWidth.endKey = "1FFFFFFFE";
  assert(rejected(engine, invalidWidth));

  auto tooSmall = request;
  tooSmall.endKey = "10000FFFF";
  assert(rejected(engine, tooSmall));

  auto invalidPublicKey = request;
  invalidPublicKey.publicKey = "synthetic-public-key";
  assert(rejected(engine, invalidPublicKey));

  auto invalidDevice = request;
  invalidDevice.device = -1;
  assert(rejected(engine, invalidDevice));

  auto conflictingOutput = request;
  conflictingOutput.outputFile =
      "/tmp/openpuzzle-kangaroo/RESULTS.TXT";
  assert(rejected(engine, conflictingOutput));

  std::cout << "KangarooEngineCommandTests passed\n";
  return 0;
}

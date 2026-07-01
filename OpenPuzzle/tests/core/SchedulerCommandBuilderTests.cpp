#include "openpuzzle/core/Scheduler.hpp"

#include <filesystem>

using namespace openpuzzle;

int main() {
  Scheduler scheduler;

  auto workspace = scheduler.workspaceForJob(42);

  if (workspace.find("000042") == std::string::npos)
    return 1;

  PuzzleRecord puzzle;
  puzzle.address = "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU";

  RangeRecord range;
  range.startKey = "400000000000000000";
  range.endKey = "4000000000000000FF";

  auto output = (std::filesystem::path(workspace) / "found.txt").string();

  auto command = scheduler.buildBitCrackCommand(
      "/usr/local/bin/cuBitCrack", puzzle, range, 0, 256, 256, 1024, output);

  if (command.find("/usr/local/bin/cuBitCrack") == std::string::npos)
    return 2;
  if (command.find("1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU") == std::string::npos)
    return 3;
  if (command.find("--keyspace 400000000000000000:4000000000000000FF") ==
      std::string::npos)
    return 4;
  if (command.find("--out ") == std::string::npos)
    return 5;
  if (command.find("-d 0") == std::string::npos)
    return 6;
  if (command.find("-b 256") == std::string::npos)
    return 7;
  if (command.find("-t 256") == std::string::npos)
    return 8;
  if (command.find("-p 1024") == std::string::npos)
    return 9;

  return 0;
}

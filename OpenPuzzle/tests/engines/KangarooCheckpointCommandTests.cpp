#include "openpuzzle/engines/EngineLaunchRequest.hpp"
#include "openpuzzle/engines/kangaroo/KangarooEngine.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

using namespace openpuzzle;

namespace {

std::string readFile(const std::filesystem::path &path) {
  std::ifstream input(path);
  return {
      std::istreambuf_iterator<char>(input),
      std::istreambuf_iterator<char>()};
}

bool contains(const std::string &text, const std::string &part) {
  return text.find(part) != std::string::npos;
}

EngineLaunchRequest requestFor(const std::filesystem::path &workspace) {
  EngineLaunchRequest request;
  request.engine = "Kangaroo";
  request.backend = "CUDA";
  request.publicKey =
      "031f6a332d3c5c4f2de2378c012f429cd109ba07d69690c6c701b6bb87860d6640";
  request.walkSeed = "0123456789ABCDEF";
  request.startKey = "100000000";
  request.endKey = "1FFFFFFFF";
  request.device = 0;
  request.workspace = workspace.string();
  request.outputFile = (workspace / "found.txt").string();
  request.logFile = (workspace / "kangaroo.log").string();
  return request;
}

} // namespace

int main() {
  const auto workspace =
      std::filesystem::temp_directory_path() /
      ("openpuzzle-kangaroo-checkpoint-" + std::to_string(getpid()));
  std::filesystem::create_directories(workspace);

  const auto executor = workspace / "synthetic-kangaroo";
  const auto arguments = workspace / "arguments.txt";
  const auto calls = workspace / "calls.txt";
  const auto checkpoint = workspace / "kangaroo.checkpoint";

  {
    std::ofstream script(executor);
    script
        << "#!/bin/sh\n"
        << "printf '%s\\n' \"$*\" > arguments.txt\n"
        << "printf '%s\\n' called >> calls.txt\n"
        << "savefile=''\n"
        << "while [ \"$#\" -gt 0 ]; do\n"
        << "  if [ \"$1\" = '-savefile' ]; then savefile=$2; shift 2; continue; fi\n"
        << "  shift\n"
        << "done\n"
        << "printf '%s\\n' checkpoint-data > \"$savefile\"\n"
        << "chmod 666 \"$savefile\"\n"
        << "exit 0\n";
  }

  std::filesystem::permissions(
      executor,
      std::filesystem::perms::owner_read |
          std::filesystem::perms::owner_write |
          std::filesystem::perms::owner_exec,
      std::filesystem::perm_options::replace);

  KangarooEngine engine(executor.string());
  const auto request = requestFor(workspace);
  const auto command = engine.buildCommand(request);

  const auto first = std::system(command.c_str());
  assert(WIFEXITED(first));
  assert(WEXITSTATUS(first) == 0);
  assert(std::filesystem::is_regular_file(checkpoint));

  auto mode = std::filesystem::status(checkpoint).permissions();
  assert((mode & std::filesystem::perms::group_all) ==
         std::filesystem::perms::none);
  assert((mode & std::filesystem::perms::others_all) ==
         std::filesystem::perms::none);

  const auto firstArguments = readFile(arguments);
  assert(contains(firstArguments, "-seed 0123456789ABCDEF"));
  assert(contains(firstArguments, "-checkpoint 1"));
  assert(contains(firstArguments, "-savefile " + checkpoint.string()));
  assert(!contains(firstArguments, "-loadwild"));

  const auto second = std::system(command.c_str());
  assert(WIFEXITED(second));
  assert(WEXITSTATUS(second) == 0);

  const auto secondArguments = readFile(arguments);
  assert(contains(secondArguments, "-loadwild " + checkpoint.string()));

  const auto safeTarget = workspace / "outside.checkpoint";
  {
    std::ofstream output(safeTarget);
    output << "outside\n";
  }

  std::filesystem::remove(checkpoint);
  std::filesystem::create_symlink(safeTarget, checkpoint);
  const auto callsBefore = readFile(calls);

  const auto unsafe = std::system(command.c_str());
  assert(WIFEXITED(unsafe));
  assert(WEXITSTATUS(unsafe) == 70);
  assert(readFile(calls) == callsBefore);
  assert(readFile(safeTarget) == "outside\n");

  std::filesystem::remove_all(workspace);
  std::cout << "KangarooCheckpointCommandTests passed\n";
  return 0;
}

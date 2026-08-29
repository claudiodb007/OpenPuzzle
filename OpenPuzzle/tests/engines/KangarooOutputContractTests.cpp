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

int main() {
  const auto workspace =
      std::filesystem::temp_directory_path() /
      ("openpuzzle-kangaroo-output-" +
       std::to_string(getpid()));
  std::filesystem::create_directories(workspace);
  std::filesystem::permissions(
      workspace,
      std::filesystem::perms::owner_all,
      std::filesystem::perm_options::replace);

  const auto executor = workspace / "synthetic-kangaroo";
  {
    std::ofstream script(executor);
    script
        << "#!/bin/sh\n"
        << "printf '%s\\n' 'PRIVATE KEY: synthetic-private-key' > RESULTS.TXT\n"
        << "exit 7\n";
  }
  std::filesystem::permissions(
      executor,
      std::filesystem::perms::owner_read |
          std::filesystem::perms::owner_write |
          std::filesystem::perms::owner_exec,
      std::filesystem::perm_options::replace);

  EngineLaunchRequest request;
  request.engine = "Kangaroo";
  request.backend = "CUDA";
  request.publicKey =
      "031f6a332d3c5c4f2de2378c012f429cd109ba07d69690c6c701b6bb87860d6640";
  request.startKey = "100000000";
  request.endKey = "1FFFFFFFF";
  request.device = 0;
  request.workspace = workspace.string();
  request.outputFile = (workspace / "found.txt").string();
  request.logFile = (workspace / "kangaroo.log").string();

  KangarooEngine engine(executor.string());
  const auto status = std::system(engine.buildCommand(request).c_str());

  assert(WIFEXITED(status));
  assert(WEXITSTATUS(status) == 7);
  assert(std::filesystem::exists(workspace / "RESULTS.TXT"));
  assert(std::filesystem::exists(request.outputFile));

  std::ifstream found(request.outputFile);
  const std::string contents{
      std::istreambuf_iterator<char>(found),
      std::istreambuf_iterator<char>()};
  assert(contents == "PRIVATE KEY: synthetic-private-key\n");

  const auto permissions =
      std::filesystem::status(request.outputFile).permissions();
  assert((permissions & std::filesystem::perms::group_all) ==
         std::filesystem::perms::none);
  assert((permissions & std::filesystem::perms::others_all) ==
         std::filesystem::perms::none);

  std::filesystem::remove_all(workspace);
  std::cout << "KangarooOutputContractTests passed\n";
  return 0;
}

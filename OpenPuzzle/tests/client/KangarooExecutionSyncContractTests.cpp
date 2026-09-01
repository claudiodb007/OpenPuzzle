#include "openpuzzle/client/ExecutionSyncService.hpp"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

using namespace openpuzzle::client;

namespace {

bool closeTo(double actual, double expected) {
  return std::fabs(actual - expected) < 0.000001;
}

} // namespace

int main() {
  const auto workspace =
      std::filesystem::temp_directory_path() /
      ("openpuzzle-kangaroo-sync-" + std::to_string(getpid()));

  std::filesystem::remove_all(workspace);
  std::filesystem::create_directories(workspace);

  {
    std::ofstream bitcrack(workspace / "bitcrack.log");
    bitcrack << "[00:00:01] 999.00 MKey/s 999\n";

    std::ofstream kangaroo(workspace / "kangaroo.log");
    kangaroo
        << "HUNT: Speed: 2.75 GKeys/s | DPs: 42M | Time: 0d 00h 05m\n"
        << "Exit completed.\n"
        << "Reached end of keyspace\n";
  }

  const auto progress =
      ExecutionSyncService::latestProgress(
          workspace.string(),
          "Kangaroo");

  assert(progress);
  assert(closeTo(progress->speedMKeys, 2750.0));
  assert(progress->keysChecked == "0");

  const auto aliasProgress =
      ExecutionSyncService::latestProgress(
          workspace.string(),
          "PSCKangaroo");

  assert(aliasProgress);
  assert(closeTo(aliasProgress->speedMKeys, 2750.0));
  assert(aliasProgress->keysChecked == "0");

  assert(!ExecutionSyncService::hasCompletionProof(
      workspace.string(),
      "Kangaroo"));

  assert(!ExecutionSyncService::hasCompletionProof(
      workspace.string(),
      "PSCKangaroo"));

  std::filesystem::remove_all(workspace);
  std::cout << "KangarooExecutionSyncContractTests passed\n";
  return 0;
}

#include "openpuzzle/runtime/KangarooRecoveryLeaseGuard.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <unistd.h>

using namespace openpuzzle;

namespace {

client::ClientExecutionState stateFor(
    const std::filesystem::path &workspace) {
  client::ClientExecutionState state;
  state.active = true;
  state.assignmentId = "11111111-1111-4111-8111-111111111111";
  state.clientId = "22222222-2222-4222-8222-222222222222";
  state.puzzle = 140;
  state.rangeId = 9001;
  state.pid = 12345;
  state.engine = "Kangaroo";
  state.backend = "CUDA";
  state.workspace = workspace.string();
  return state;
}

std::string readFile(const std::filesystem::path &path) {
  std::ifstream input(path);
  return {
      std::istreambuf_iterator<char>(input),
      std::istreambuf_iterator<char>()};
}

} // namespace

int main() {
  const auto root =
      std::filesystem::temp_directory_path() /
      ("openpuzzle-kangaroo-lease-" + std::to_string(getpid()));
  const auto workspace = root / "workspace";
  const auto bin = root / "bin";
  const auto capture = root / "curl-arguments.txt";

  std::filesystem::remove_all(root);
  std::filesystem::create_directories(workspace);
  std::filesystem::create_directories(bin);

  {
    std::ofstream log(workspace / "kangaroo.log");
    log << "HUNT: Speed: 2.75 GKeys/s | DPs: 42M | Time: 0d 01h 05m\n";
  }

  const auto curl = bin / "curl";
  {
    std::ofstream script(curl);
    script
        << "#!/bin/sh\n"
        << "printf '%s\\n' \"$*\" > \"$OPENPUZZLE_FAKE_CURL_CAPTURE\"\n"
        << "case \"$OPENPUZZLE_FAKE_CURL_MODE\" in\n"
        << "  accepted) exit 0 ;;\n"
        << "  rejected) printf '%s\\n' '{\"error\":\"assignment_lease_expired\",\"message\":\"expired\"}'; exit 22 ;;\n"
        << "  *) printf '%s\\n' 'temporary network failure'; exit 28 ;;\n"
        << "esac\n";
  }
  std::filesystem::permissions(
      curl,
      std::filesystem::perms::owner_all,
      std::filesystem::perm_options::replace);

  const char *oldPathValue = std::getenv("PATH");
  const std::string oldPath = oldPathValue ? oldPathValue : "";
  const std::string testPath = bin.string() + ":" + oldPath;
  assert(setenv("PATH", testPath.c_str(), 1) == 0);
  assert(setenv("OPENPUZZLE_FAKE_CURL_CAPTURE", capture.c_str(), 1) == 0);

  const auto state = stateFor(workspace);

  assert(setenv("OPENPUZZLE_FAKE_CURL_MODE", "accepted", 1) == 0);
  const auto accepted =
      KangarooRecoveryLeaseGuard::verify("https://server.test", state);
  assert(accepted.accepted());
  assert(accepted.speedMKeys == 2750.0);

  const auto arguments = readFile(capture);
  assert(arguments.find("/api/range/progress") != std::string::npos);
  assert(arguments.find("\"keys_checked\":\"0\"") !=
         std::string::npos);
  assert(arguments.find("\"speed_mkeys\":2750") !=
         std::string::npos);

  assert(setenv("OPENPUZZLE_FAKE_CURL_MODE", "rejected", 1) == 0);
  const auto rejected =
      KangarooRecoveryLeaseGuard::verify("https://server.test", state);
  assert(rejected.status == KangarooRecoveryLeaseStatus::Rejected);

  assert(setenv("OPENPUZZLE_FAKE_CURL_MODE", "temporary", 1) == 0);
  const auto retry =
      KangarooRecoveryLeaseGuard::verify("https://server.test", state);
  assert(retry.status == KangarooRecoveryLeaseStatus::Retry);

  std::filesystem::remove(workspace / "kangaroo.log");
  const auto missingTelemetry =
      KangarooRecoveryLeaseGuard::verify("https://server.test", state);
  assert(missingTelemetry.status == KangarooRecoveryLeaseStatus::Invalid);

  auto bitcrack = state;
  bitcrack.engine = "BitCrack";
  assert(KangarooRecoveryLeaseGuard::verify(
             "https://server.test", bitcrack).status ==
         KangarooRecoveryLeaseStatus::Invalid);

  assert(setenv("PATH", oldPath.c_str(), 1) == 0);
  unsetenv("OPENPUZZLE_FAKE_CURL_CAPTURE");
  unsetenv("OPENPUZZLE_FAKE_CURL_MODE");
  std::filesystem::remove_all(root);

  std::cout << "KangarooRecoveryLeaseGuardTests passed\n";
  return 0;
}

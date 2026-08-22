#include "openpuzzle/client/ExecutionSyncService.hpp"
#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/performance/AdaptiveProfileUpdater.hpp"
#include "openpuzzle/performance/GpuProfileManager.hpp"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

using namespace openpuzzle;
using namespace openpuzzle::client;
using namespace openpuzzle::performance;

namespace {

bool near(double actual, double expected) {
  return std::abs(actual - expected) < 0.0001;
}

ClientExecutionState managedState() {
  ClientExecutionState state;
  state.active = true;
  state.assignmentId = "11111111-1111-4111-8111-111111111111";
  state.clientId = "22222222-2222-4222-8222-222222222222";
  state.puzzle = 71;
  state.rangeId = 1508;
  state.pid = 12345;
  state.device = 0;
  state.blocks = 224;
  state.threads = 128;
  state.points = 1024;
  state.profileManaged = true;
  state.gpuName = "NVIDIA GeForce RTX 4070 SUPER";
  state.engine = "BitCrack";
  state.backend = "CUDA";
  state.workspace = "/tmp/openpuzzle-adaptive-profile";
  return state;
}

GpuProfileRecord initialProfile() {
  GpuProfileRecord profile;
  profile.gpuName = "NVIDIA GeForce RTX 4070 SUPER";
  profile.backend = "CUDA";
  profile.engine = "BitCrack";
  profile.blocks = 224;
  profile.threads = 128;
  profile.points = 1024;
  profile.averageSpeed = 1291.0;
  profile.minimumSpeed = 1200.0;
  profile.maximumSpeed = 1400.0;
  profile.samples = 5;
  return profile;
}

} // namespace

int main() {
  const auto root =
      std::filesystem::temp_directory_path() /
      ("openpuzzle-adaptive-profile-" +
       std::to_string(getpid()));
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);

  Database database;
  assert(database.open((root / "openpuzzle.db").string()));
  assert(database.createSchema());

  GpuProfileManager profiles(database);
  assert(profiles.save(initialProfile()));

  AdaptiveProfileUpdater updater(database);
  const auto state = managedState();
  const auto result = updater.update(
      state,
      {900.0, 1000.0, 1480.0, 1488.0, 1490.0, 1487.0, 1489.0});

  assert(result.attempted);
  assert(result.updated);
  assert(result.acceptedSamples == 5);
  assert(near(result.sustainedSpeed, 1488.0));
  assert(near(result.calibratedPlanningSpeed, 1329.09));

  const auto saved = profiles.load(
      state.gpuName,
      state.backend,
      state.engine);
  assert(saved);
  assert(near(saved->averageSpeed, 1329.09));
  assert(near(saved->minimumSpeed, 1200.0));
  assert(near(saved->maximumSpeed, 1488.0));
  assert(saved->samples == 10);

  auto unmanaged = state;
  unmanaged.profileManaged = false;
  const auto skipped = updater.update(
      unmanaged,
      {1500.0, 1500.0, 1500.0, 1500.0, 1500.0, 1500.0, 1500.0});
  assert(!skipped.attempted);
  assert(!skipped.updated);

  auto mismatch = state;
  mismatch.points = 256;
  const auto rejected = updater.update(
      mismatch,
      {1500.0, 1500.0, 1500.0, 1500.0, 1500.0, 1500.0, 1500.0});
  assert(rejected.attempted);
  assert(!rejected.updated);
  assert(!rejected.error.empty());

  const auto workspace = root / "workspace";
  std::filesystem::create_directories(workspace);
  std::ofstream log(workspace / "bitcrack.log");
  for (int index = 0; index < 7; ++index) {
    log << "GPU | 1 target "
        << (1400 + index)
        << ".00 MKey/s ("
        << (1000000 * (index + 1))
        << " total) [00:00:01]\r";
  }
  log.close();

  const auto samples =
      ExecutionSyncService::speedSamples(
          workspace.string(),
          "BitCrack");
  assert(samples.size() == 7);
  assert(near(samples.front(), 1400.0));
  assert(near(samples.back(), 1406.0));

  database.close();
  std::filesystem::remove_all(root);
  std::cout << "AdaptiveProfileUpdaterTests: OK\n";
  return 0;
}

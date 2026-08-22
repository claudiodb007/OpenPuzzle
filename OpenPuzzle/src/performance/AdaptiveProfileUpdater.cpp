#include "openpuzzle/performance/AdaptiveProfileUpdater.hpp"

#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/performance/AdaptiveCalibration.hpp"
#include "openpuzzle/performance/GpuProfileManager.hpp"

#include <algorithm>
#include <limits>

namespace openpuzzle::performance {

AdaptiveProfileUpdater::AdaptiveProfileUpdater(
    Database &database)
    : database_(database) {}

AdaptiveProfileUpdateResult
AdaptiveProfileUpdater::update(
    const client::ClientExecutionState &state,
    const std::vector<double> &speedSamples) const {
  AdaptiveProfileUpdateResult result;
  result.attempted = state.profileManaged;

  if (!result.attempted) {
    return result;
  }

  if (state.gpuName.empty() ||
      state.backend.empty() ||
      state.engine.empty() ||
      state.blocks <= 0 ||
      state.threads <= 0 ||
      state.points <= 0) {
    result.error =
        "Incomplete managed GPU profile metadata";
    return result;
  }

  GpuProfileManager profiles(database_);
  const auto existing = profiles.load(
      state.gpuName,
      state.backend,
      state.engine);

  if (!existing) {
    result.error =
        "Managed GPU profile was not found";
    return result;
  }

  if (existing->blocks != state.blocks ||
      existing->threads != state.threads ||
      existing->points != state.points) {
    result.error =
        "Completed launch does not match the managed GPU profile";
    return result;
  }

  const AdaptiveCalibration calibration;
  const auto calibrated = calibration.calibrate(
      existing->averageSpeed,
      speedSamples);

  result.sustainedSpeed =
      calibrated.sustainedSpeed;
  result.calibratedPlanningSpeed =
      calibrated.calibratedPlanningSpeed;
  result.acceptedSamples =
      calibrated.acceptedSamples;

  if (!calibrated.updated) {
    result.error =
        "Not enough sustained speed samples";
    return result;
  }

  auto updated = *existing;
  updated.averageSpeed =
      calibrated.calibratedPlanningSpeed;

  if (updated.minimumSpeed <= 0.0 ||
      calibrated.sustainedSpeed < updated.minimumSpeed) {
    updated.minimumSpeed =
        calibrated.sustainedSpeed;
  }

  if (calibrated.sustainedSpeed >
      updated.maximumSpeed) {
    updated.maximumSpeed =
        calibrated.sustainedSpeed;
  }

  const auto maximumSamples =
      static_cast<std::size_t>(
          std::numeric_limits<int>::max());
  const auto currentSamples =
      static_cast<std::size_t>(
          std::max(0, updated.samples));
  const auto available =
      maximumSamples -
      std::min(maximumSamples, currentSamples);
  const auto increment =
      std::min(available, calibrated.acceptedSamples);
  updated.samples =
      static_cast<int>(currentSamples + increment);

  if (!profiles.save(updated)) {
    result.error =
        "Unable to save the calibrated GPU profile";
    return result;
  }

  result.updated = true;
  return result;
}

} // namespace openpuzzle::performance

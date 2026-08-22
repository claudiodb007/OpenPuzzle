#include "openpuzzle/performance/AdaptiveCalibration.hpp"

#include <algorithm>
#include <utility>

namespace openpuzzle::performance {
namespace {

constexpr double DefaultLiveWeight = 0.25;
constexpr double DefaultPlanningFactor = 0.97;
constexpr double DefaultMaximumChangeFraction = 0.15;

double validFraction(double value, double fallback) {
  return value > 0.0 && value <= 1.0 ? value : fallback;
}

} // namespace

AdaptiveCalibrationResult AdaptiveCalibration::calibrate(
    double currentPlanningSpeed,
    const std::vector<double> &liveSamples,
    AdaptiveCalibrationPolicy policy) const {
  AdaptiveCalibrationResult result;
  result.previousPlanningSpeed = currentPlanningSpeed;
  result.calibratedPlanningSpeed = currentPlanningSpeed;

  std::vector<double> validSamples;
  validSamples.reserve(liveSamples.size());
  for (const double sample : liveSamples) {
    if (sample > 0.0) {
      validSamples.push_back(sample);
    }
  }

  if (validSamples.size() <= policy.warmupSamples) {
    return result;
  }

  std::vector<double> sustainedSamples(
      validSamples.begin() + static_cast<std::ptrdiff_t>(policy.warmupSamples),
      validSamples.end());
  result.acceptedSamples = sustainedSamples.size();
  if (result.acceptedSamples < policy.minimumSamples) {
    return result;
  }

  result.sustainedSpeed = median(std::move(sustainedSamples));
  const double planningFactor = validFraction(
      policy.planningFactor, DefaultPlanningFactor);
  result.candidatePlanningSpeed = result.sustainedSpeed * planningFactor;

  if (currentPlanningSpeed <= 0.0) {
    result.calibratedPlanningSpeed = result.candidatePlanningSpeed;
    result.updated = result.calibratedPlanningSpeed > 0.0;
    return result;
  }

  const double liveWeight = validFraction(policy.liveWeight, DefaultLiveWeight);
  const double maximumChangeFraction = validFraction(
      policy.maximumChangeFraction, DefaultMaximumChangeFraction);
  const double blended =
      currentPlanningSpeed * (1.0 - liveWeight) +
      result.candidatePlanningSpeed * liveWeight;
  const double lowerBound =
      currentPlanningSpeed * (1.0 - maximumChangeFraction);
  const double upperBound =
      currentPlanningSpeed * (1.0 + maximumChangeFraction);

  result.calibratedPlanningSpeed =
      std::max(lowerBound, std::min(blended, upperBound));
  result.updated = true;
  return result;
}

double AdaptiveCalibration::median(std::vector<double> values) {
  std::sort(values.begin(), values.end());
  const std::size_t middle = values.size() / 2;
  if (values.size() % 2 == 0) {
    return (values[middle - 1] + values[middle]) / 2.0;
  }
  return values[middle];
}

} // namespace openpuzzle::performance

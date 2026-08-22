#pragma once

#include <cstddef>
#include <vector>

namespace openpuzzle::performance {

struct AdaptiveCalibrationPolicy {
  std::size_t warmupSamples{2};
  std::size_t minimumSamples{5};
  double liveWeight{0.25};
  double planningFactor{0.97};
  double maximumChangeFraction{0.15};
};

struct AdaptiveCalibrationResult {
  bool updated{false};
  double previousPlanningSpeed{0.0};
  double sustainedSpeed{0.0};
  double candidatePlanningSpeed{0.0};
  double calibratedPlanningSpeed{0.0};
  std::size_t acceptedSamples{0};
};

class AdaptiveCalibration {
public:
  AdaptiveCalibrationResult calibrate(
      double currentPlanningSpeed,
      const std::vector<double> &liveSamples,
      AdaptiveCalibrationPolicy policy = {}) const;

private:
  static double median(std::vector<double> values);
};

} // namespace openpuzzle::performance

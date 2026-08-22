#include "openpuzzle/performance/AdaptiveCalibration.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using openpuzzle::performance::AdaptiveCalibration;

namespace {

void expectNear(double actual, double expected, const std::string &message) {
  if (std::abs(actual - expected) > 0.0001) {
    std::cerr << "FAIL: " << message << ": expected " << expected
              << ", got " << actual << '\n';
    std::exit(1);
  }
}

void expect(bool condition, const std::string &message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

void stableSamplesUseMedianAndGradualBlend() {
  AdaptiveCalibration calibration;
  const auto result = calibration.calibrate(
      1291.0, {900.0, 1000.0, 1480.0, 1488.0, 1490.0, 1487.0, 1489.0});
  expect(result.updated, "stable samples should update the profile");
  expect(result.acceptedSamples == 5, "five sustained samples are accepted");
  expectNear(result.sustainedSpeed, 1488.0, "median sustained speed");
  expectNear(result.candidatePlanningSpeed, 1443.36,
             "three percent planning margin");
  expectNear(result.calibratedPlanningSpeed, 1329.09,
             "twenty-five percent gradual blend");
}

void outlierDoesNotDistortSustainedSpeed() {
  AdaptiveCalibration calibration;
  const auto result = calibration.calibrate(
      1400.0, {700.0, 900.0, 1480.0, 1488.0, 1490.0, 8000.0, 1487.0});
  expectNear(result.sustainedSpeed, 1488.0, "median rejects a large outlier");
}

void insufficientSamplesKeepExistingProfile() {
  AdaptiveCalibration calibration;
  const auto result = calibration.calibrate(
      1400.0, {800.0, 900.0, 1480.0, 1488.0, 1490.0, 1487.0});
  expect(!result.updated, "four sustained samples are insufficient");
  expectNear(result.calibratedPlanningSpeed, 1400.0,
             "existing profile remains unchanged");
}

void invalidSamplesAreIgnored() {
  AdaptiveCalibration calibration;
  const auto result = calibration.calibrate(
      1400.0,
      {0.0, -10.0, 800.0, 900.0, 1480.0, 1488.0, 1490.0, 1487.0, 1489.0});
  expect(result.updated, "valid samples still update the profile");
  expect(result.acceptedSamples == 5, "invalid readings do not count");
  expectNear(result.sustainedSpeed, 1488.0, "valid median after warm-up");
}

void upwardChangeIsClamped() {
  AdaptiveCalibration calibration;
  const auto result = calibration.calibrate(
      1000.0, {500.0, 600.0, 2000.0, 2000.0, 2000.0, 2000.0, 2000.0});
  expectNear(result.calibratedPlanningSpeed, 1150.0,
             "upward change is limited to fifteen percent");
}

void downwardChangeIsClamped() {
  AdaptiveCalibration calibration;
  const auto result = calibration.calibrate(
      1000.0, {500.0, 600.0, 100.0, 100.0, 100.0, 100.0, 100.0});
  expectNear(result.calibratedPlanningSpeed, 850.0,
             "downward change is limited to fifteen percent");
}

void missingProfileUsesConservativeLiveSpeed() {
  AdaptiveCalibration calibration;
  const auto result = calibration.calibrate(
      0.0, {500.0, 600.0, 1500.0, 1500.0, 1500.0, 1500.0, 1500.0});
  expect(result.updated, "live data can initialize a missing speed");
  expectNear(result.calibratedPlanningSpeed, 1455.0,
             "new profile uses the conservative live speed");
}

} // namespace

int main() {
  stableSamplesUseMedianAndGradualBlend();
  outlierDoesNotDistortSustainedSpeed();
  insufficientSamplesKeepExistingProfile();
  invalidSamplesAreIgnored();
  upwardChangeIsClamped();
  downwardChangeIsClamped();
  missingProfileUsesConservativeLiveSpeed();
  std::cout << "AdaptiveCalibrationTests: OK\n";
  return 0;
}

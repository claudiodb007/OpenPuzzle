#pragma once
#include "openpuzzle/performance/AutoTuner.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <stdexcept>

namespace openpuzzle {
struct WindowsAutoBenchmarkResult {
 BenchmarkResult validated;
 int candidatesTested=0,validationSeconds=60;
 double planningSpeed=0;
};
inline bool sustainedWindowsBenchmark(const BenchmarkResult& r) {
 return r.success&&r.samples>=3&&std::isfinite(r.averageSpeed)&&r.averageSpeed>0&&r.averageSpeed<=1e9&&
  std::isfinite(r.minimumSpeed)&&std::isfinite(r.maximumSpeed)&&r.minimumSpeed>0&&
  r.minimumSpeed<=r.averageSpeed&&r.averageSpeed<=r.maximumSpeed;
}
// The Linux matrix, memory budget, stability score and planning margin are shared.
// The runner returns no value for a rejected candidate. Exceptions abort the
// operation, including cancellation; an incomplete run never yields a profile.
inline WindowsAutoBenchmarkResult runWindowsAutoBenchmark(int computeUnits,int memoryMiB,bool cuda,int seconds,
 const std::function<std::optional<BenchmarkResult>(const BenchmarkConfiguration&,int,bool)>& run) {
 if(computeUnits<1||computeUnits>4096||memoryMiB<256||memoryMiB>1048576||seconds<10||seconds>30||!run)
  throw std::invalid_argument("Automatic benchmark requires verified GPU capabilities and 10..30 seconds per candidate");
 AutoTuner tuner;auto matrix=tuner.defaultMatrix(computeUnits,memoryMiB,cuda);
 std::vector<BenchmarkResult> results;
 for(const auto& candidate:matrix) {
  // Unlike the legacy fallback, never exceed a known device's memory budget.
  if(AutoTuner::estimatedMemoryMb(candidate.configuration)>memoryMiB*70/100)continue;
  auto value=run(candidate.configuration,seconds,false);
  if(value&&sustainedWindowsBenchmark(*value)) {
   value->configuration=candidate.configuration;results.push_back(*value);
  }
 }
 const auto best=tuner.selectBest(results);
 if(!best.success)throw std::runtime_error("No sustained automatic benchmark candidate; previous profiles preserved");
 auto final=run(best.configuration,60,true);
 if(!final||!sustainedWindowsBenchmark(*final))throw std::runtime_error("Automatic benchmark finalist failed validation; previous profiles preserved");
 final->configuration=best.configuration;
 WindowsAutoBenchmarkResult result;result.validated=*final;result.validationSeconds=60;
 for(const auto& c:matrix)if(AutoTuner::estimatedMemoryMb(c.configuration)<=memoryMiB*70/100)++result.candidatesTested;
 result.planningSpeed=AutoTuner::planningSpeed(*final);return result;
}
}

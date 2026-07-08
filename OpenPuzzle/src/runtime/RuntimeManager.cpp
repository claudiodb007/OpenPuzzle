#include "openpuzzle/runtime/RuntimeManager.hpp"

namespace openpuzzle {

RuntimeManager::RuntimeManager() = default;

ExecutionResult RuntimeManager::run(const ExecutionContext& context,
                                    int maxSeconds,
                                    int maxSamples) const {
  return executionManager_.run(context, maxSeconds, maxSamples);
}

} // namespace openpuzzle

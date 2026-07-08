#include "openpuzzle/engines/EngineMonitor.hpp"

namespace openpuzzle {

EngineMonitor::EngineMonitor() = default;

void EngineMonitor::consumeLine(const std::string& line,
                                ExecutionResult& result,
                                const ProgressCallback& onProgress) const {
  auto parsed = parser_.parse(line);

  if (parsed.type == bitcrack::ParsedLineType::Speed) {
    result.averageSpeed = parsed.speedMKeys;

    if (parsed.speedMKeys >= 100.0) {
      result.speedSamples.push_back(parsed.speedMKeys);
    }

    if (!parsed.totalKeys.empty()) {
      result.keysChecked = parsed.totalKeys;
    }

    if (onProgress) {
      onProgress(result);
    }
  }

  if (parsed.type == bitcrack::ParsedLineType::Found) {
    result.keyFound = true;
    result.privateKey = parsed.value;
  }
}

} // namespace openpuzzle

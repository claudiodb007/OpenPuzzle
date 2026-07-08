#pragma once

#include "openpuzzle/adapters/bitcrack/BitCrackOutputParser.hpp"
#include "openpuzzle/core/ExecutionResult.hpp"

#include <functional>
#include <string>

namespace openpuzzle {

class EngineMonitor {
public:
  using ProgressCallback = std::function<void(const ExecutionResult&)>;

  EngineMonitor();

  void consumeLine(const std::string& line,
                   ExecutionResult& result,
                   const ProgressCallback& onProgress = nullptr) const;

private:
  bitcrack::BitCrackOutputParser parser_;
};

} // namespace openpuzzle

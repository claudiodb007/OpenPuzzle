#pragma once

#include "openpuzzle/engines/EngineOutputParser.hpp"
#include "openpuzzle/runtime/Execution.hpp"

#include <memory>
#include <functional>
#include <string>

namespace openpuzzle {

using ProgressCallback =
    std::function<void(const ExecutionProgress&)>;


class ExecutionMonitor {
public:
  explicit ExecutionMonitor(std::unique_ptr<EngineOutputParser> parser);

  void processLine(Execution& execution, const std::string& line);

  void setCallback(ProgressCallback callback);

private:

  ProgressCallback callback_;
  std::unique_ptr<EngineOutputParser> parser_;
};

} // namespace openpuzzle

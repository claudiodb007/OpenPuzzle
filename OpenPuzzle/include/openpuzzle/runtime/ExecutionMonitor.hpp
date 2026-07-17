#pragma once

#include "openpuzzle/engines/EngineOutputParser.hpp"
#include "openpuzzle/runtime/ExecutionProgress.hpp"

#include <functional>
#include <memory>
#include <string>

namespace openpuzzle {

class Execution;

class ExecutionMonitor {
public:
  using Callback = std::function<void(const ExecutionProgress&)>;

  explicit ExecutionMonitor(std::unique_ptr<EngineOutputParser> parser);

  void setCallback(Callback callback);
  void processLine(Execution& execution, const std::string& line);

private:
  std::unique_ptr<EngineOutputParser> parser_;
  Callback callback_;
};

} // namespace openpuzzle

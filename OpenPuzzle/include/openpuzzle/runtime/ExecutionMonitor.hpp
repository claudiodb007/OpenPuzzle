#pragma once

#include "openpuzzle/engines/EngineOutputParser.hpp"
#include "openpuzzle/runtime/Execution.hpp"

#include <memory>
#include <string>

namespace openpuzzle {

class ExecutionMonitor {
public:
  explicit ExecutionMonitor(std::unique_ptr<EngineOutputParser> parser);

  void processLine(Execution& execution, const std::string& line);

private:
  std::unique_ptr<EngineOutputParser> parser_;
};

} // namespace openpuzzle

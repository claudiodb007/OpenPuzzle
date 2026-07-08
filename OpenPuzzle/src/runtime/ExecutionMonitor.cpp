#include "openpuzzle/runtime/ExecutionMonitor.hpp"

namespace openpuzzle {

ExecutionMonitor::ExecutionMonitor(
    std::unique_ptr<EngineOutputParser> parser)
    : parser_(std::move(parser)) {}

void ExecutionMonitor::processLine(
    Execution& execution,
    const std::string& line) {

  auto progress = parser_->parseLine(line);

  if (!progress)
    return;

  execution.updateProgress(*progress);
}

} // namespace openpuzzle

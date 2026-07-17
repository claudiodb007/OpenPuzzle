#include "openpuzzle/runtime/ExecutionMonitor.hpp"

#include "openpuzzle/runtime/Execution.hpp"

namespace openpuzzle {

ExecutionMonitor::ExecutionMonitor(std::unique_ptr<EngineOutputParser> parser)
    : parser_(std::move(parser)) {}

void ExecutionMonitor::setCallback(Callback callback) {
  callback_ = std::move(callback);
}

void ExecutionMonitor::processLine(Execution& execution,
                                   const std::string& line) {
  if (!parser_) {
    return;
  }

  auto progress = parser_->parseLine(line);

  if (!progress) {
    return;
  }

  execution.updateProgress(*progress);

  if (callback_) {
    callback_(*progress);
  }
}

} // namespace openpuzzle

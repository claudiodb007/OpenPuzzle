#include "openpuzzle/engines/keyhunt/KeyHuntEngine.hpp"

#include <sstream>
#include <utility>

namespace openpuzzle {

KeyHuntEngine::KeyHuntEngine(
    std::string executable)
    : executable_(std::move(executable)) {}

EngineInfo KeyHuntEngine::info() const {
  EngineInfo info;
  info.name = "KeyHunt";
  info.version = "unknown";
  info.backend = "CPU";
  info.executable = executable_;
  info.available = !executable_.empty();

  return info;
}

bool KeyHuntEngine::prepare() {
  return !executable_.empty();
}

std::string KeyHuntEngine::buildCommand(
    const EngineLaunchRequest& request) const {
  std::ostringstream command;

  command
      << executable_
      << " -m address"
      << " -f "
      << request.targetFile
      << " -r "
      << request.startKey
      << ":"
      << request.endKey;

  if (request.threads > 0) {
    command
        << " -t "
        << request.threads;
  }

  command
      << " 2>&1 | tee -a "
      << request.logFile;

  return command.str();
}

bool KeyHuntEngine::launch() {
  running_ = true;
  return true;
}

bool KeyHuntEngine::stop() {
  running_ = false;
  return true;
}

bool KeyHuntEngine::running() const {
  return running_;
}

} // namespace openpuzzle

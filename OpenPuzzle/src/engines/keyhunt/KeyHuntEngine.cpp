#include "openpuzzle/engines/keyhunt/KeyHuntEngine.hpp"

#include <sstream>
#include <utility>

namespace openpuzzle {

namespace {

std::string shellQuote(
    const std::string& value) {
  std::string quoted = "'";

  for (const char character : value) {
    if (character == '\'') {
      quoted += "'\\''";
    } else {
      quoted += character;
    }
  }

  quoted += "'";
  return quoted;
}

} // namespace

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
      << "cd "
      << shellQuote(request.workspace)
      << " && umask 077"
      << " && : > KEYFOUNDKEYFOUND.txt"
      << " && chmod 600 KEYFOUNDKEYFOUND.txt"
      << " && ln -sfn KEYFOUNDKEYFOUND.txt found.txt"
      << " && "
      << shellQuote(executable_)
      << " -m address"
      << " -f "
      << shellQuote(request.targetFile)
      << " -r "
      << shellQuote(
             request.startKey +
             ":" +
             request.endKey)
      << " -l compress"
      << " -n 1024"
      << " -q"
      << " -s 1";

  if (request.threads > 0) {
    command
        << " -t "
        << request.threads;
  }

  command
      << " >> "
      << shellQuote(request.logFile)
      << " 2>&1";

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

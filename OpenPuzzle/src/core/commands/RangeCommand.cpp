#include "openpuzzle/core/commands/RangeCommand.hpp"

#include "openpuzzle/runtime/RunSession.hpp"

namespace openpuzzle {

int RangeCommand::run(const std::vector<std::string> &args) const {
  RunSession session;

  return session.run(args);
}

} // namespace openpuzzle

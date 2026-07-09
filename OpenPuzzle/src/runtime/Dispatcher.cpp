#include "openpuzzle/runtime/Dispatcher.hpp"

namespace openpuzzle {

bool Dispatcher::dispatch(const SchedulerDecision& decision) const {
  return decision.shouldDispatch;
}

} // namespace openpuzzle

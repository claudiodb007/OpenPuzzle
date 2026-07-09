#pragma once

#include "openpuzzle/runtime/SchedulerDecision.hpp"

namespace openpuzzle {

class Dispatcher {
public:
  Dispatcher() = default;

  bool dispatch(const SchedulerDecision& decision) const;
};

} // namespace openpuzzle

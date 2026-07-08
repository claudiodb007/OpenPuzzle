#pragma once

#include "openpuzzle/config/Configuration.hpp"
#include "openpuzzle/core/EventBus.hpp"
#include "openpuzzle/core/Scheduler.hpp"
#include "openpuzzle/engines/EngineManager.hpp"
#include "openpuzzle/runtime/RuntimeManager.hpp"

namespace openpuzzle {

class ApplicationContext {
public:
  ApplicationContext();

  const Configuration& configuration() const;
  Configuration& configuration();

  Scheduler& scheduler();
  EngineManager& engineManager();
  RuntimeManager& runtimeManager();
  EventBus& eventBus();

private:
  Configuration configuration_;
  Scheduler scheduler_;
  EngineManager engineManager_;
  RuntimeManager runtimeManager_;
  EventBus eventBus_;
};

} // namespace openpuzzle

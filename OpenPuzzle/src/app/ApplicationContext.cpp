#include "openpuzzle/app/ApplicationContext.hpp"

#include "openpuzzle/config/ConfigurationManager.hpp"

namespace openpuzzle {

ApplicationContext::ApplicationContext()
    : configuration_(ConfigurationManager::load()) {}

const Configuration& ApplicationContext::configuration() const {
  return configuration_;
}

Configuration& ApplicationContext::configuration() {
  return configuration_;
}

Scheduler& ApplicationContext::scheduler() {
  return scheduler_;
}

EngineManager& ApplicationContext::engineManager() {
  return engineManager_;
}

EventBus& ApplicationContext::eventBus() {
  return eventBus_;
}

} // namespace openpuzzle

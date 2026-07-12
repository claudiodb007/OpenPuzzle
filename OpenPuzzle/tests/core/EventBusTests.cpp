#include "openpuzzle/core/EventBus.hpp"

using namespace openpuzzle;

int main() {
  EventBus bus;
  int received = 0;

  bus.subscribe([&](const Event &) { received++; });

  Event event;

  event.type =
      EventType::ExecutionStarted;

  event.executionId = 1;
  event.jobId = 1;
  event.message = "test";

  bus.publish(event);

  return received == 1 ? 0 : 1;
}

#include "openpuzzle/core/EventBus.hpp"

using namespace openpuzzle;

int main() {
    EventBus bus;
    int received = 0;

    bus.subscribe([&](const Event&) {
        received++;
    });

    bus.publish(Event{
        EventType::ExecutionStarted,
        1,
        1,
        "test",
        "",
        0.0
    });

    return received == 1 ? 0 : 1;
}

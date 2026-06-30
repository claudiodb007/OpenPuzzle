#include "openpuzzle/core/EventBus.hpp"

namespace openpuzzle {

void EventBus::subscribe(const Listener& listener) {
    listeners_.push_back(listener);
}

void EventBus::publish(const Event& event) const {
    for (const auto& listener : listeners_) {
        if (listener) {
            listener(event);
        }
    }
}

std::string EventBus::eventTypeToString(EventType type) {
    switch (type) {
        case EventType::ExecutionStarted:
            return "EXECUTION_STARTED";
        case EventType::Speed:
            return "SPEED";
        case EventType::Progress:
            return "PROGRESS";
        case EventType::FoundKey:
            return "FOUND_KEY";
        case EventType::ExecutionFinished:
            return "EXECUTION_FINISHED";
        case EventType::Error:
            return "ERROR";
    }

    return "UNKNOWN";
}

} // namespace openpuzzle

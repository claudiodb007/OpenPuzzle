#pragma once

#include <functional>
#include <string>
#include <vector>

namespace openpuzzle {

enum class EventType {
  ExecutionStarted,
  Speed,
  Progress,
  FoundKey,
  ExecutionFinished,
  Error
};

struct Event {
  EventType type = EventType::Progress;
  int executionId = 0;
  int jobId = 0;
  std::string message;
  std::string value;
  double numericValue = 0.0;
};

class EventBus {
public:
  using Listener = std::function<void(const Event &)>;

  void subscribe(const Listener &listener);
  void publish(const Event &event) const;

  static std::string eventTypeToString(EventType type);

private:
  std::vector<Listener> listeners_;
};

} // namespace openpuzzle

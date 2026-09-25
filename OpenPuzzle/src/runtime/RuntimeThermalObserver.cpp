#include "openpuzzle/runtime/RuntimeThermalObserver.hpp"

#include <cstdlib>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <utility>

namespace openpuzzle {

RuntimeThermalObserver::RuntimeThermalObserver(
    GpuThermalPolicyConfiguration policy,
    Reader reader,
    const std::chrono::seconds sampleInterval,
    const std::chrono::seconds reminderInterval)
    : policy_(std::move(policy)),
      reader_(std::move(reader)),
      sampleInterval_(sampleInterval),
      reminderInterval_(reminderInterval) {}

bool RuntimeThermalObserver::enabled() const {
  return processOwnsMonitoring() &&
         policy_.enabled &&
         GpuThermalPolicy::valid(policy_) &&
         static_cast<bool>(reader_) &&
         sampleInterval_.count() > 0 &&
         reminderInterval_.count() > 0;
}

bool RuntimeThermalObserver::protectionEnabled() const {
  return enabled() && policy_.stopOnCritical;
}

std::vector<RuntimeThermalEvent>
RuntimeThermalObserver::poll() {
  return pollAt(Clock::now());
}

std::vector<RuntimeThermalEvent>
RuntimeThermalObserver::pollAt(
    const Clock::time_point now) {
  std::vector<RuntimeThermalEvent> events;

  if (!enabled() ||
      (nextSample_ && now < *nextSample_)) {
    return events;
  }

  nextSample_ = now + sampleInterval_;

  for (const auto &snapshot : reader_()) {
    const auto current =
        GpuThermalPolicy::evaluate(
            policy_, snapshot.temperatureC);

    auto &device = states_[snapshot.deviceId];
    const auto previous = device.state;

    const bool hot =
        current == GpuThermalState::Warning ||
        current == GpuThermalState::Critical;

    const bool wasHot =
        previous == GpuThermalState::Warning ||
        previous == GpuThermalState::Critical;

    if (hot) {
      const bool transition = current != previous;
      const bool reminder =
          !transition &&
          device.lastNotice &&
          now - *device.lastNotice >= reminderInterval_;

      if (transition || reminder) {
        RuntimeThermalEvent event;
        event.kind =
            current == GpuThermalState::Critical
                ? RuntimeThermalEventKind::Critical
                : RuntimeThermalEventKind::Warning;
        event.snapshot = snapshot;
        event.thresholdC =
            current == GpuThermalState::Critical
                ? policy_.criticalC
                : policy_.warningC;
        event.reminder = reminder;
        events.push_back(std::move(event));
        device.lastNotice = now;
      }
    } else if (
        current == GpuThermalState::Normal &&
        wasHot) {
      RuntimeThermalEvent event;
      event.kind = RuntimeThermalEventKind::Recovered;
      event.snapshot = snapshot;
      events.push_back(std::move(event));
      device.lastNotice.reset();
    }

    device.state = current;
  }

  return events;
}

bool RuntimeThermalObserver::processOwnsMonitoring() {
  const char *value = std::getenv(OwnerEnvironment);
  return value == nullptr || std::string(value) != "0";
}

void RuntimeThermalObserver::print(
    const RuntimeThermalEvent &event,
    std::ostream &output) {
  const bool recovered =
      event.kind == RuntimeThermalEventKind::Recovered;
  const bool critical =
      event.kind == RuntimeThermalEventKind::Critical;

  std::ostringstream message;

  message
      << '\n'
      << "OpenPuzzle thermal "
      << (recovered ? "recovery" : "warning")
      << '\n'
      << "--------------------------\n"
      << "Device.............. "
      << event.snapshot.vendor << ' '
      << event.snapshot.deviceId << '\n'
      << std::fixed << std::setprecision(1);

  if (event.snapshot.temperatureC) {
    message
        << "Temperature......... "
        << *event.snapshot.temperatureC
        << " C\n";
  } else {
    message
        << "Temperature......... unavailable\n";
  }

  message
      << "State............... "
      << (recovered
              ? "NORMAL"
              : (critical ? "CRITICAL" : "WARNING"));

  if (event.reminder) {
    message << " (reminder)";
  }

  message << '\n';

  if (!recovered) {
    message
        << "Threshold........... "
        << event.thresholdC
        << " C\n";
  }

  message
      << "Action.............. "
      << (recovered
              ? "monitoring continues"
              : "diagnostic only; execution continues")
      << '\n';

  output << message.str();
}

} // namespace openpuzzle

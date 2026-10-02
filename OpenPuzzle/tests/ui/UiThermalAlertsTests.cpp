#include "ThermalAlerts.hpp"

#include <cassert>
#include <iostream>

using namespace openpuzzle::ui;

namespace {

QString status(
    const QString& state,
    const QString& temperature = "76.0 C",
    const QString& slot = "cuda-0") {
  return QString(
      "OpenPuzzle Status\n"
      "-----------------\n\n"
      "Slot............... %1\n"
      "Status............. running\n"
      "Backend............ CUDA\n"
      "Temperature........ %2\n"
      "Thermal state....... %3\n")
      .arg(slot, temperature, state);
}

} // namespace

int main() {
  ThermalAlertTracker tracker;

  auto update = tracker.update(status("NORMAL", "70.0 C"));
  assert(update.level == ThermalAlertLevel::None);
  assert(update.activeDevices.isEmpty());
  assert(update.transitions.isEmpty());

  update = tracker.update(status("WARNING"));
  assert(update.level == ThermalAlertLevel::Warning);
  assert(update.activeDevices.size() == 1);
  assert(update.activeDevices[0].name == "CUDA-0");
  assert(update.activeDevices[0].temperature == "76.0 C");
  assert(update.transitions.size() == 1);
  assert(
      update.transitions[0].kind ==
      ThermalAlertTransitionKind::Warning);

  update = tracker.update(status("WARNING", "77.0 C"));
  assert(update.level == ThermalAlertLevel::Warning);
  assert(update.transitions.isEmpty());

  update = tracker.update(status("CRITICAL", "86.0 C"));
  assert(update.level == ThermalAlertLevel::Critical);
  assert(update.transitions.size() == 1);
  assert(
      update.transitions[0].kind ==
      ThermalAlertTransitionKind::Critical);

  update = tracker.update(status("NORMAL", "72.0 C"));
  assert(update.level == ThermalAlertLevel::None);
  assert(update.activeDevices.isEmpty());
  assert(update.transitions.size() == 1);
  assert(
      update.transitions[0].kind ==
      ThermalAlertTransitionKind::Recovered);
  assert(update.transitions[0].device.name == "CUDA-0");

  update = tracker.update(status("NORMAL", "71.0 C"));
  assert(update.transitions.isEmpty());

  update = tracker.update(status("INVALID", "unavailable"));
  assert(update.level == ThermalAlertLevel::Invalid);
  assert(update.transitions.size() == 1);
  assert(
      update.transitions[0].kind ==
      ThermalAlertTransitionKind::Invalid);

  const QString multiple =
      status("WARNING", "78.0 C", "cuda-0") + "\n" +
      status("CRITICAL", "86.5 C", "cuda-1");
  tracker.reset();
  update = tracker.update(multiple);
  assert(update.level == ThermalAlertLevel::Critical);
  assert(update.activeDevices.size() == 2);
  assert(update.transitions.size() == 2);

  tracker.reset();
  update = tracker.update(
      "OpenPuzzle Status\n"
      "-----------------\n\n"
      "Slot............... cpu\n"
      "Status............. running\n"
      "Backend............ CPU\n");
  assert(update.level == ThermalAlertLevel::None);
  assert(update.transitions.isEmpty());

  std::cout << "UiThermalAlertsTests passed\n";
  return 0;
}

#include "ThermalHistory.hpp"

#include <QFile>
#include <QTemporaryDir>

#include <cassert>
#include <iostream>

using namespace openpuzzle::ui;

namespace {

ThermalAlertTransition transition(
    const ThermalAlertTransitionKind kind,
    const QString& device,
    const QString& temperature) {
  ThermalAlertDevice alertDevice;
  alertDevice.name = device;
  alertDevice.identity = device.toLower();
  alertDevice.temperature = temperature;
  return {kind, alertDevice};
}

} // namespace

int main() {
  QTemporaryDir temporary;
  assert(temporary.isValid());
  const QString path =
      temporary.filePath("thermal-history.json");
  const QDateTime first = QDateTime::fromString(
      "2026-10-02T12:00:00.000Z",
      Qt::ISODateWithMs);

  ThermalHistoryStore history(path, 3);
  assert(history.load());
  assert(history.entries().isEmpty());

  assert(history.append(
      {transition(
          ThermalAlertTransitionKind::Warning,
          "CUDA-0",
          "76.0 C")},
      false,
      first));
  assert(history.entries().size() == 1);
  assert(history.entries()[0].deviceName == "CUDA-0");
  assert(!history.entries()[0].stopRequested);

  // Reopening the interface while the same state remains active must not
  // duplicate the last persisted device transition.
  assert(history.append(
      {transition(
          ThermalAlertTransitionKind::Warning,
          "CUDA-0",
          "77.0 C")},
      false,
      first.addSecs(30)));
  assert(history.entries().size() == 1);

  assert(history.append(
      {transition(
          ThermalAlertTransitionKind::Critical,
          "CUDA-0",
          "86.0 C")},
      true,
      first.addSecs(60)));
  assert(history.entries().size() == 2);
  assert(history.entries()[1].stopRequested);

  assert(history.append(
      {transition(
          ThermalAlertTransitionKind::Recovered,
          "CUDA-0",
          "72.0 C")},
      false,
      first.addSecs(90)));
  assert(history.entries().size() == 3);

  assert(history.append(
      {transition(
          ThermalAlertTransitionKind::Invalid,
          "OPENCL",
          "unavailable")},
      false,
      first.addSecs(120)));
  assert(history.entries().size() == 3);
  assert(
      history.entries()[0].kind ==
      ThermalAlertTransitionKind::Critical);
  assert(
      history.entries()[2].kind ==
      ThermalAlertTransitionKind::Invalid);

  ThermalHistoryStore restored(path, 3);
  assert(restored.load());
  assert(restored.entries().size() == 3);
  assert(restored.entries()[0].deviceName == "CUDA-0");
  assert(restored.entries()[0].stopRequested);
  assert(restored.entries()[2].deviceName == "OPENCL");

  QFile permissions(path);
  const auto mode = permissions.permissions();
  assert(mode.testFlag(QFileDevice::ReadOwner));
  assert(mode.testFlag(QFileDevice::WriteOwner));
  assert(!mode.testFlag(QFileDevice::ReadGroup));
  assert(!mode.testFlag(QFileDevice::ReadOther));

  assert(restored.clear());
  assert(restored.entries().isEmpty());
  assert(!QFile::exists(path));

  std::cout << "UiThermalHistoryTests passed\n";
  return 0;
}

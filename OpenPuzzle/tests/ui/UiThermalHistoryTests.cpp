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

  const auto verifyUnreadableHistory = [&](const QByteArray& contents) {
    QFile original(path);
    assert(original.open(QIODevice::WriteOnly));
    assert(original.write(contents) == contents.size());
    original.close();

    ThermalHistoryStore protectedHistory(path, 3);
    assert(!protectedHistory.load());
    assert(protectedHistory.needsRecovery());
    assert(!protectedHistory.append(
        {transition(
            ThermalAlertTransitionKind::Warning,
            "CUDA-0",
            "77.0 C")},
        false,
        first));

    QFile preserved(path);
    assert(preserved.open(QIODevice::ReadOnly));
    assert(preserved.readAll() == contents);
    preserved.close();

    assert(protectedHistory.clear());
    assert(!protectedHistory.needsRecovery());
    assert(protectedHistory.append(
        {transition(
            ThermalAlertTransitionKind::Warning,
            "CUDA-0",
            "77.0 C")},
        false,
        first));
    assert(protectedHistory.entries().size() == 1);
    assert(protectedHistory.clear());
  };

  verifyUnreadableHistory("{malformed JSON");
  verifyUnreadableHistory(
      "{\"version\":2,\"entries\":[]}");

  // Valid JSON with a damaged entry must not be silently filtered and saved
  // over the original history when the next transition arrives.
  verifyUnreadableHistory("{\"version\":1,\"entries\":[42]}");
  verifyUnreadableHistory("{\"version\":1,\"entries\":[{\"kind\":\"warning\"}]}");
  verifyUnreadableHistory(
      "{\"version\":1,\"entries\":[{\"timestamp_utc\":\"2026-10-02T12:00:00.000Z\","
      "\"device\":\"CUDA-0\",\"temperature\":\"76.0 C\",\"kind\":\"warning\","
      "\"stop_requested\":false},null]}");
  verifyUnreadableHistory(
      "{\"version\":1,\"entries\":[{\"timestamp_utc\":\"2026-10-02T12:00:00.000Z\","
      "\"device\":\"CUDA-0\",\"temperature\":\"76.0 C\",\"kind\":\"warning\","
      "\"stop_requested\":\"true\"}]}");

  std::cout << "UiThermalHistoryTests passed\n";
  return 0;
}

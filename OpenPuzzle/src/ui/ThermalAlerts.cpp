#include "ThermalAlerts.hpp"

#include <QHash>
#include <QRegularExpression>
#include <QStringList>

#include <utility>

namespace openpuzzle::ui {

namespace {

struct StatusBlock {
  QHash<QString, QString> fields;
};

QVector<StatusBlock> parseStatusBlocks(const QString& output) {
  static const QRegularExpression fieldPattern(
      R"(^(.+?)\.{2,}\s*(.*)$)");

  QVector<StatusBlock> blocks;
  StatusBlock current;

  for (const QString& line : output.split('\n')) {
    const auto match = fieldPattern.match(line.trimmed());
    if (!match.hasMatch()) {
      continue;
    }

    const QString key = match.captured(1).trimmed();
    const QString value = match.captured(2).trimmed();

    if (key == "Slot" && !current.fields.isEmpty()) {
      blocks.push_back(current);
      current.fields.clear();
    }

    current.fields.insert(key, value);
  }

  if (!current.fields.isEmpty()) {
    blocks.push_back(current);
  }

  return blocks;
}

ThermalAlertLevel alertLevel(QString state) {
  state = state.trimmed().toLower();
  if (state == "critical") {
    return ThermalAlertLevel::Critical;
  }
  if (state == "invalid") {
    return ThermalAlertLevel::Invalid;
  }
  if (state == "warning") {
    return ThermalAlertLevel::Warning;
  }
  return ThermalAlertLevel::None;
}

int priority(const ThermalAlertLevel level) {
  switch (level) {
  case ThermalAlertLevel::Critical:
    return 3;
  case ThermalAlertLevel::Invalid:
    return 2;
  case ThermalAlertLevel::Warning:
    return 1;
  case ThermalAlertLevel::None:
  default:
    return 0;
  }
}

ThermalAlertTransitionKind transitionKind(
    const ThermalAlertLevel level) {
  switch (level) {
  case ThermalAlertLevel::Critical:
    return ThermalAlertTransitionKind::Critical;
  case ThermalAlertLevel::Invalid:
    return ThermalAlertTransitionKind::Invalid;
  case ThermalAlertLevel::Warning:
  case ThermalAlertLevel::None:
  default:
    return ThermalAlertTransitionKind::Warning;
  }
}

ThermalAlertDevice deviceFromBlock(
    const StatusBlock& block,
    const ThermalAlertLevel level) {
  QString name = block.fields.value("Slot").trimmed();
  if (name.isEmpty()) {
    name = block.fields.value("Backend").trimmed();
  }
  if (name.isEmpty()) {
    name = QStringLiteral("PRIMARY");
  } else {
    name = name.toUpper();
  }

  ThermalAlertDevice device;
  device.name = name;
  device.identity = name.toLower();
  device.temperature =
      block.fields.value("Temperature").trimmed();
  device.level = level;
  return device;
}

} // namespace

ThermalAlertUpdate ThermalAlertTracker::update(
    const QString& statusOutput) {
  ThermalAlertUpdate result;
  QHash<QString, ThermalAlertDevice> nextActive;

  for (const auto& block : parseStatusBlocks(statusOutput)) {
    if (!block.fields.contains("Thermal state")) {
      continue;
    }

    const QString rawState =
        block.fields.value("Thermal state").trimmed().toLower();
    const ThermalAlertLevel level = alertLevel(rawState);
    const ThermalAlertDevice device =
        deviceFromBlock(block, level);
    const auto previous = activeDevices_.constFind(device.identity);

    if (level != ThermalAlertLevel::None) {
      nextActive.insert(device.identity, device);
      result.activeDevices.push_back(device);

      if (previous == activeDevices_.cend() ||
          previous->level != level) {
        result.transitions.push_back(
            {transitionKind(level), device});
      }

      if (priority(level) > priority(result.level)) {
        result.level = level;
      }
      continue;
    }

    if (rawState == "normal" &&
        previous != activeDevices_.cend()) {
      ThermalAlertDevice recovered = device;
      recovered.level = previous->level;
      result.transitions.push_back(
          {ThermalAlertTransitionKind::Recovered,
           recovered});
    }
  }

  activeDevices_ = std::move(nextActive);
  return result;
}

void ThermalAlertTracker::reset() {
  activeDevices_.clear();
}

} // namespace openpuzzle::ui

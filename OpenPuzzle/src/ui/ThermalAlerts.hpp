#pragma once

#include <QHash>
#include <QString>
#include <QVector>

namespace openpuzzle::ui {

enum class ThermalAlertLevel {
  None,
  Warning,
  Invalid,
  Critical,
};

enum class ThermalAlertTransitionKind {
  Warning,
  Invalid,
  Critical,
  Recovered,
};

struct ThermalAlertDevice {
  QString identity;
  QString name;
  QString temperature;
  ThermalAlertLevel level = ThermalAlertLevel::None;
};

struct ThermalAlertTransition {
  ThermalAlertTransitionKind kind =
      ThermalAlertTransitionKind::Warning;
  ThermalAlertDevice device;
};

struct ThermalAlertUpdate {
  ThermalAlertLevel level = ThermalAlertLevel::None;
  QVector<ThermalAlertDevice> activeDevices;
  QVector<ThermalAlertTransition> transitions;
};

class ThermalAlertTracker final {
public:
  ThermalAlertUpdate update(const QString& statusOutput);
  void reset();

private:
  QHash<QString, ThermalAlertDevice> activeDevices_;
};

} // namespace openpuzzle::ui

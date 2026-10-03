#pragma once

#include "ThermalAlerts.hpp"

#include <QDateTime>
#include <QString>
#include <QVector>

namespace openpuzzle::ui {

struct ThermalHistoryEntry {
  QDateTime timestampUtc;
  ThermalAlertTransitionKind kind =
      ThermalAlertTransitionKind::Warning;
  QString deviceName;
  QString temperature;
  bool stopRequested = false;
};

class ThermalHistoryStore final {
public:
  explicit ThermalHistoryStore(
      QString path = defaultPath(),
      int maximumEntries = 200);

  bool load();
  bool append(
      const QVector<ThermalAlertTransition>& transitions,
      bool stopRequested,
      const QDateTime& timestampUtc =
          QDateTime::currentDateTimeUtc());
  bool clear();

  const QVector<ThermalHistoryEntry>& entries() const;
  QString path() const;
  bool needsRecovery() const;

  static QString defaultPath();

private:
  bool save() const;
  bool isDuplicate(
      const ThermalAlertTransition& transition) const;

  QString path_;
  int maximumEntries_ = 200;
  QVector<ThermalHistoryEntry> entries_;
  bool needsRecovery_ = false;
};

} // namespace openpuzzle::ui

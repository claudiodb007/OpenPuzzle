#include "ThermalHistory.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

#include <utility>

namespace openpuzzle::ui {

namespace {

QString kindName(const ThermalAlertTransitionKind kind) {
  switch (kind) {
  case ThermalAlertTransitionKind::Critical:
    return QStringLiteral("critical");
  case ThermalAlertTransitionKind::Invalid:
    return QStringLiteral("invalid");
  case ThermalAlertTransitionKind::Recovered:
    return QStringLiteral("recovered");
  case ThermalAlertTransitionKind::Warning:
  default:
    return QStringLiteral("warning");
  }
}

bool kindFromName(
    const QString& name,
    ThermalAlertTransitionKind& kind) {
  if (name == "warning") {
    kind = ThermalAlertTransitionKind::Warning;
    return true;
  }
  if (name == "critical") {
    kind = ThermalAlertTransitionKind::Critical;
    return true;
  }
  if (name == "invalid") {
    kind = ThermalAlertTransitionKind::Invalid;
    return true;
  }
  if (name == "recovered") {
    kind = ThermalAlertTransitionKind::Recovered;
    return true;
  }
  return false;
}

} // namespace

ThermalHistoryStore::ThermalHistoryStore(
    QString path,
    const int maximumEntries)
    : path_(std::move(path)),
      maximumEntries_(qMax(1, maximumEntries)) {}

bool ThermalHistoryStore::load() {
  QFile file(path_);
  if (!file.exists()) {
    entries_.clear();
    needsRecovery_ = false;
    return true;
  }
  if (!file.open(QIODevice::ReadOnly)) {
    needsRecovery_ = true;
    return false;
  }

  QJsonParseError error;
  const QJsonDocument document =
      QJsonDocument::fromJson(file.readAll(), &error);
  if (error.error != QJsonParseError::NoError ||
      !document.isObject()) {
    needsRecovery_ = true;
    return false;
  }

  const QJsonObject root = document.object();
  if (root.value("version").toInt() != 1 ||
      !root.value("entries").isArray()) {
    needsRecovery_ = true;
    return false;
  }

  QVector<ThermalHistoryEntry> loaded;
  for (const QJsonValue& value :
       root.value("entries").toArray()) {
    if (!value.isObject()) {
      needsRecovery_ = true;
      return false;
    }

    const QJsonObject object = value.toObject();
    if ((!object.value("temperature").isUndefined() &&
         !object.value("temperature").isString()) ||
        (!object.value("stop_requested").isUndefined() &&
         !object.value("stop_requested").isBool())) {
      needsRecovery_ = true;
      return false;
    }
    ThermalHistoryEntry entry;
    entry.timestampUtc = QDateTime::fromString(
        object.value("timestamp_utc").toString(),
        Qt::ISODateWithMs);
    entry.deviceName =
        object.value("device").toString().trimmed();
    entry.temperature =
        object.value("temperature").toString().trimmed();
    entry.stopRequested =
        object.value("stop_requested").toBool(false);

    if (!entry.timestampUtc.isValid() ||
        entry.deviceName.isEmpty() ||
        !kindFromName(
            object.value("kind").toString(),
            entry.kind)) {
      needsRecovery_ = true;
      return false;
    }

    entry.timestampUtc = entry.timestampUtc.toUTC();
    loaded.push_back(entry);
  }

  if (loaded.size() > maximumEntries_) {
    loaded.remove(
        0,
        loaded.size() - maximumEntries_);
  }
  entries_ = std::move(loaded);
  needsRecovery_ = false;
  return true;
}

bool ThermalHistoryStore::append(
    const QVector<ThermalAlertTransition>& transitions,
    const bool stopRequested,
    const QDateTime& timestampUtc) {
  if (transitions.isEmpty()) {
    return true;
  }

  if (needsRecovery_) {
    return false;
  }

  const QVector<ThermalHistoryEntry> previous = entries_;
  bool changed = false;
  for (const auto& transition : transitions) {
    if (transition.device.name.trimmed().isEmpty() ||
        isDuplicate(transition)) {
      continue;
    }

    entries_.push_back({
        timestampUtc.toUTC(),
        transition.kind,
        transition.device.name.trimmed(),
        transition.device.temperature.trimmed(),
        transition.kind == ThermalAlertTransitionKind::Critical &&
            stopRequested,
    });
    changed = true;
  }

  if (!changed) {
    return true;
  }

  if (entries_.size() > maximumEntries_) {
    entries_.remove(
        0,
        entries_.size() - maximumEntries_);
  }

  if (save()) {
    return true;
  }

  entries_ = previous;
  return false;
}

bool ThermalHistoryStore::clear() {
  if (QFile::exists(path_) && !QFile::remove(path_)) {
    return false;
  }
  entries_.clear();
  needsRecovery_ = false;
  return true;
}

const QVector<ThermalHistoryEntry>&
ThermalHistoryStore::entries() const {
  return entries_;
}

QString ThermalHistoryStore::path() const {
  return path_;
}

bool ThermalHistoryStore::needsRecovery() const {
  return needsRecovery_;
}

QString ThermalHistoryStore::defaultPath() {
  return QStandardPaths::writableLocation(
             QStandardPaths::GenericDataLocation) +
      "/OpenPuzzle/thermal-history.json";
}

bool ThermalHistoryStore::save() const {
  const QFileInfo fileInfo(path_);
  if (!QDir().mkpath(fileInfo.absolutePath())) {
    return false;
  }

  QJsonArray values;
  for (const auto& entry : entries_) {
    QJsonObject object;
    object.insert(
        "timestamp_utc",
        entry.timestampUtc.toUTC().toString(Qt::ISODateWithMs));
    object.insert("kind", kindName(entry.kind));
    object.insert("device", entry.deviceName);
    object.insert("temperature", entry.temperature);
    object.insert("stop_requested", entry.stopRequested);
    values.push_back(object);
  }

  QJsonObject root;
  root.insert("version", 1);
  root.insert("entries", values);

  QSaveFile file(path_);
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }
  if (file.write(QJsonDocument(root).toJson(
          QJsonDocument::Indented)) < 0 ||
      !file.commit()) {
    return false;
  }

  return QFile::setPermissions(
      path_,
      QFileDevice::ReadOwner |
          QFileDevice::WriteOwner);
}

bool ThermalHistoryStore::isDuplicate(
    const ThermalAlertTransition& transition) const {
  for (auto found = entries_.crbegin();
       found != entries_.crend();
       ++found) {
    if (found->deviceName.compare(
            transition.device.name,
            Qt::CaseInsensitive) != 0) {
      continue;
    }
    return found->kind == transition.kind;
  }
  return false;
}

} // namespace openpuzzle::ui

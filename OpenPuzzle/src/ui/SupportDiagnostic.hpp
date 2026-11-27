#pragma once

#include <QHash>
#include <QRegularExpression>
#include <QStringList>
#include <QVector>

namespace openpuzzle::ui {

inline QStringList diagnosticGpus(const QString& inventory) {
  struct Gpu {
    QString backend;
    int index;
    QString name;
    QString driver;
  };

  static const QRegularExpression backendPattern(
      R"(^(CUDA|OpenCL)(?: devices\.{2,}\s*\d+|: (?:DETECTED|NO GPU|NOT INSTALLED|UNAVAILABLE).*)$)");
  static const QRegularExpression indexPattern(
      R"(^(?:Inventory index: (\d{1,3})$|Physical device (\d{1,3}):))");
  static const QRegularExpression linuxDevicePattern(R"(^Device\.{2,}\s*(\d{1,3}):\s*(.+)$)");
  static const QRegularExpression modelPattern(R"(^[\p{L}\p{N} .,_()+\-]{1,96}$)");
  static const QRegularExpression identifierPattern(
      R"([a-z0-9_]{24,}|[a-f0-9]{8}(?:-[a-f0-9]{4}){3}-[a-f0-9]{12}|(?:sk|gh[pousr]|token|secret|password)[_-])",
      QRegularExpression::CaseInsensitiveOption);
  static const QRegularExpression driverPattern(
      R"(^(?:CUDA API )?[0-9]{1,6}(?:\.[0-9]{1,6}){1,5}(?: [-\w(),.]{1,32})?$)");

  QVector<Gpu> gpus;
  QHash<QString, int> positions;
  QString backend;
  QStringList adapters;
  bool windowsAdapters = false;
  int current = -1;
  const auto switchDevice = [&](int index) {
    const QString key = backend + ':' + QString::number(index);
    current = positions.value(key, -1);
    if (current < 0 && !backend.isEmpty() && gpus.size() < 24) {
      current = static_cast<int>(gpus.size());
      positions.insert(key, current);
      gpus.push_back({backend, index, {}, {}});
    }
  };
  const auto shareableModel = [&](const QString& name) {
    return modelPattern.match(name).hasMatch() && !identifierPattern.match(name).hasMatch();
  };
  const auto setModel = [&](const QString& name) {
    if (current >= 0 && shareableModel(name)) {
      gpus[current].name = name;
    }
  };

  for (const QString& rawLine : inventory.split('\n')) {
    const QString line = rawLine.trimmed();
    if (line == "GPU telemetry") {
      break;
    }
    if (line.startsWith("Windows display adapters:")) {
      windowsAdapters = true;
      continue;
    }
    if (windowsAdapters) {
      if (line.startsWith("Windows adapter: ")) {
        const auto adapter = line.mid(17);
        if (adapters.size() < 24 && shareableModel(adapter) && !adapters.contains(adapter)) {
          adapters.push_back(adapter);
        }
      }
      continue;
    }
    if (const auto match = backendPattern.match(line); match.hasMatch()) {
      backend = match.captured(1);
      current = -1;
    } else if (const auto match = indexPattern.match(line); match.hasMatch()) {
      switchDevice((match.captured(1).isEmpty() ? match.captured(2) : match.captured(1)).toInt());
    } else if (const auto match = linuxDevicePattern.match(line); match.hasMatch()) {
      switchDevice(match.captured(1).toInt());
      setModel(match.captured(2).trimmed());
    } else if (line.startsWith("Device: ")) {
      setModel(line.mid(8));
    } else if (line.startsWith("Display name: ")) {
      setModel(line.mid(14));
    } else if (current >= 0 && (line.startsWith("Driver: ") || line.startsWith("Driver.."))) {
      const QString driver = line.mid(6).remove(QRegularExpression(R"(^[:.]+\s*)")).trimmed();
      if (driverPattern.match(driver).hasMatch() && !identifierPattern.match(driver).hasMatch()) {
        gpus[current].driver = driver;
      }
    }
  }

  QStringList lines;
  for (const auto& gpu : gpus) {
    if (gpu.name.isEmpty() && gpu.driver.isEmpty()) {
      continue;
    }
    QString line = QStringLiteral("%1 %2").arg(gpu.backend).arg(gpu.index);
    if (!gpu.name.isEmpty()) {
      line += " | " + gpu.name;
      adapters.removeAll(gpu.name);
    }
    if (!gpu.driver.isEmpty()) {
      line += " | Driver: " + gpu.driver;
    }
    lines.push_back(line);
  }
  for (const auto& adapter : adapters) {
    lines << "Display adapter | " + adapter;
  }
  return lines;
}

inline QStringList diagnosticErrors(const QString& log) {
  // Copy fixed categories and validated numeric codes, never a raw error line.
  static const QStringList serverCodes = {
      "assignment_not_found", "assignment_lease_expired", "assignment_conflict",
      "client_not_registered", "client_not_available", "client_has_active_assignment",
      "claim_conflict", "incomplete_assignment_coverage", "invalid_keys_checked",
      "solution_pending_verification", "puzzle_solved"};
  static const QRegularExpression serverCode(R"json("error"\s*:\s*"([a-z_]+)")json");
  static const QRegularExpression networkCode(R"(\bcurl: \((\d{1,2})\))");
  static const QRegularExpression doctorCode(R"(\bOP-DOCTOR-(\d{3})\b)");
  static const QRegularExpression failure(
      R"(^(?:windows (?:run|cli) refused:|windows desktop:|error:|failed:|reason\.{2,}|problem\.{2,}|.*\b(?:failed|refused|timed out)\b))",
      QRegularExpression::CaseInsensitiveOption);
  static const QRegularExpression leaseRefusal(
      R"(Vanity lease refused)", QRegularExpression::CaseInsensitiveOption);
  static const QRegularExpression heartbeatFailure(
      R"(^Heartbeat\.{2,}\s*failed$)", QRegularExpression::CaseInsensitiveOption);
  static const QRegularExpression timeout(
      R"(\b(?:timed out|timeout)\b)", QRegularExpression::CaseInsensitiveOption);

  QStringList errors;
  const auto lines = log.right(32 * 1024).split('\n');
  for (auto it = lines.crbegin(); it != lines.crend() && errors.size() < 6; ++it) {
    const QString line = it->trimmed();
    QString error;
    if (const auto match = serverCode.match(line); serverCodes.contains(match.captured(1))) {
      error = match.captured(1);
    } else if (const auto match = networkCode.match(line); match.hasMatch()) {
      error = "cURL error " + match.captured(1);
    } else if (const auto match = doctorCode.match(line); match.hasMatch()) {
      error = "OP-DOCTOR-" + match.captured(1);
    } else if (leaseRefusal.match(line).hasMatch()) {
      error = "Vanity lease refused";
    } else if (heartbeatFailure.match(line).hasMatch()) {
      error = "Heartbeat failed";
    } else if (timeout.match(line).hasMatch() && failure.match(line).hasMatch()) {
      error = "Operation timed out";
    } else if (failure.match(line).hasMatch()) {
      error = "Operation failed (details omitted)";
    }
    if (!error.isEmpty() && !errors.contains(error)) {
      errors.push_back(error);
    }
  }
  return errors;
}

} // namespace openpuzzle::ui

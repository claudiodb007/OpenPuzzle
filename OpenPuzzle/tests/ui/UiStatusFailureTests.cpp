#include "MainWindow.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QElapsedTimer>
#include <QEvent>
#include <QEventLoop>
#include <QFile>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>

#include <cassert>
#include <functional>

using openpuzzle::ui::MainWindow;

namespace {

template<typename Widget>
Widget* control(MainWindow& window, const char* id) {
  for (auto* widget : window.findChildren<Widget*>()) {
    if (widget->property("controlId").toString() == id) {
      return widget;
    }
  }
  return nullptr;
}

bool waitUntil(const std::function<bool()>& condition) {
  QElapsedTimer timer;
  timer.start();
  do {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    if (condition()) {
      return true;
    }
    QThread::msleep(5);
  } while (timer.elapsed() < 5000);
  return condition();
}

void writeFile(const QString& path, const QByteArray& contents) {
  QFile file(path);
  assert(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  assert(file.write(contents) == contents.size());
}

QByteArray readFile(const QString& path) {
  QFile file(path);
  assert(file.open(QIODevice::ReadOnly));
  return file.readAll();
}

void stopAutomaticRefresh(MainWindow& window) {
  for (auto* timer : window.findChildren<QTimer*>()) {
    if (!timer->isSingleShot() && timer->interval() == 3000) {
      timer->stop();
    }
  }
}

void shortenStatusTimeout(MainWindow& window) {
  bool found = false;
  for (auto* timer : window.findChildren<QTimer*>()) {
    if (timer->isSingleShot() && timer->interval() == 10000) {
      timer->setInterval(75);
      found = true;
    }
  }
  assert(found);
}

int visibleSlots(MainWindow& window) {
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  int count = 0;
  for (auto* widget : window.findChildren<QWidget*>()) {
    if (widget->property("controlId").toString() == "runtimeSlot" &&
        widget->isVisible()) {
      ++count;
    }
  }
  return count;
}

} // namespace

int main(int argc, char* argv[]) {
  QApplication application(argc, argv);
  QCoreApplication::setApplicationName("OpenPuzzleUiStatusFailureTests");
  QCoreApplication::setOrganizationName("OpenPuzzle Project Tests");
  QTemporaryDir isolated;
  assert(isolated.isValid());
  qputenv("HOME", isolated.path().toUtf8());
  qputenv("XDG_CONFIG_HOME", isolated.filePath("config").toUtf8());
  qputenv("XDG_DATA_HOME", isolated.filePath("data").toUtf8());
  QSettings().clear();

  const QString cliPath = isolated.filePath("openpuzzle");
  const QString modePath = isolated.filePath("mode");
  const QString responsePath = isolated.filePath("response");
  const QString callsPath = isolated.filePath("calls");
  const QString holdPath = isolated.filePath("hold");
  writeFile(callsPath, {});
  writeFile(modePath, "good\n");
  writeFile(responsePath, "Status............. idle\nExecution.......... none\n");
  writeFile(cliPath,
      "#!/bin/sh\n"
      "printf '%s\\n' \"$*\" >> \"$UI_STATUS_CALLS\"\n"
      "[ \"$1\" = status ] || exit 0\n"
      "mode=$(/bin/cat \"$UI_STATUS_MODE\")\n"
      "case \"$mode\" in\n"
      " error) printf 'Synthetic status failure\\n' >&2; exit 7;;\n"
      " empty) exit 0;;\n"
      " good|error-idle|crash|hold|read-error) /bin/cat \"$UI_STATUS_RESPONSE\";;\n"
      "esac\n"
      "case \"$mode\" in\n"
      " error-idle) exit 7;;\n"
      " crash) kill -TERM \"$$\";;\n"
      " hold) while :; do /bin/sleep 0.01; done;;\n"
      " read-error) while [ -f \"$UI_STATUS_HOLD\" ]; do /bin/sleep 0.01; done;;\n"
      "esac\n");
  assert(QFile::setPermissions(cliPath,
      QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
  qputenv("OPENPUZZLE_CLI", cliPath.toUtf8());
  qputenv("UI_STATUS_MODE", modePath.toUtf8());
  qputenv("UI_STATUS_RESPONSE", responsePath.toUtf8());
  qputenv("UI_STATUS_CALLS", callsPath.toUtf8());
  qputenv("UI_STATUS_HOLD", holdPath.toUtf8());
  qputenv("OPENPUZZLE_UI_RUNTIME_LOG", isolated.filePath("runtime.log").toUtf8());

  MainWindow window;
  window.show();
  stopAutomaticRefresh(window);
  auto* start = control<QPushButton>(window, "start");
  auto* refresh = control<QPushButton>(window, "refresh");
  auto* badge = control<QLabel>(window, "statusBadge");
  auto* details = control<QPlainTextEdit>(window, "statusOutput");
  auto* process = window.findChild<QProcess*>();
  assert(start && refresh && badge && details && process);
  const bool startupStartEnabled = start->isEnabled();
  assert(waitUntil([&]() {
    return process->state() == QProcess::NotRunning && badge->text() == "Stopped";
  }));
  assert(start->isEnabled());

  const auto query = [&](const QByteArray& mode, const QByteArray& response) {
    writeFile(modePath, mode);
    writeFile(responsePath, response);
    const int before = readFile(callsPath).count("status\n");
    refresh->click();
    assert(waitUntil([&]() {
      return process->state() == QProcess::NotRunning &&
             readFile(callsPath).count("status\n") > before;
    }));
  };
  const QByteArray running =
      "Slot............... opencl-1\nStatus............. running\n"
      "Assignment......... confirmed-assignment\nEngine............. BitCrack\n"
      "Backend............ OpenCL\nSpeed.............. 200.0 MKey/s\n"
      "Temperature........ 76.0 C\nThermal state....... WARNING\n";
  query("good", running);
  assert(badge->text() == "Running" && visibleSlots(window) == 1);
  auto* thermalBanner = control<QWidget>(window, "thermalAlertBanner");
  auto* thermalHistory = control<QPlainTextEdit>(window, "thermalHistoryOutput");
  assert(thermalBanner && !thermalBanner->isHidden() && thermalHistory);
  const QString confirmedHistory = thermalHistory->toPlainText();
  query("error", {});
  qInfo() << "Startup Start enabled:" << startupStartEnabled;
  qInfo() << "After failed query, Start enabled:" << start->isEnabled()
          << "badge:" << badge->text();
  assert(!start->isEnabled());
  assert(!startupStartEnabled);

  const auto expectUnavailable = [&]() {
    assert(badge->text() == "Status unavailable");
    assert(badge->property("statusUnavailable").toBool());
    assert(details->toPlainText() == QString::fromUtf8(running).trimmed());
    assert(visibleSlots(window) == 1);
    assert(!thermalBanner->isHidden());
    assert(thermalHistory->toPlainText() == confirmedHistory);
    for (const char* id : {"start", "benchmark", "selfTest", "installKangaroo", "saveThermal"}) {
      auto* button = control<QPushButton>(window, id);
      assert(button && !button->isEnabled());
      const auto before = readFile(callsPath);
      button->click();
      assert(readFile(callsPath) == before);
    }
    assert(!control<QCheckBox>(window, "thermalEnabled")->isEnabled());
    for (const char* id : {"refresh", "doctor", "updates", "audit", "safeStop", "stop"}) {
      assert(control<QPushButton>(window, id)->isEnabled());
    }
  };
  expectUnavailable();
  for (const QByteArray response : {
           QByteArray("Synthetic malformed output\n"),
           QByteArray("Status............. unexpected\n"),
           QByteArray("Runtime PID........ invalid\n"),
           QByteArray("Slot............... cuda-0\nStatus............. idle\n\n"
                      "Slot............... opencl-1\nAssignment......... incomplete\n")}) {
    query("good", response);
    expectUnavailable();
  }
  query("empty", {});
  expectUnavailable();
  query("error-idle", "Status............. idle\nExecution.......... none\n");
  expectUnavailable();
  query("crash", "Status............. idle\nExecution.......... none\n");
  expectUnavailable();

  // A pipe read error must remain a failure even if the child later exits 0.
  writeFile(holdPath, {});
  writeFile(modePath, "read-error");
  writeFile(responsePath, "Status............. idle\nExecution.......... none\n");
  refresh->click();
  assert(waitUntil([&]() { return process->state() == QProcess::Running; }));
  assert(QMetaObject::invokeMethod(process, "errorOccurred", Qt::DirectConnection,
      Q_ARG(QProcess::ProcessError, QProcess::ReadError)));
  assert(QFile::remove(holdPath));
  assert(waitUntil([&]() { return process->state() == QProcess::NotRunning; }));
  assert(process->exitStatus() == QProcess::NormalExit && process->exitCode() == 0);
  expectUnavailable();

  shortenStatusTimeout(window);
  query("hold", "Status............. idle\nExecution.......... none\n");
  expectUnavailable();
  auto* summary = control<QLabel>(window, "statusSummary");
  assert(summary && summary->text().contains("exceeded 10 seconds"));

  QComboBox* language = nullptr;
  for (auto* combo : window.findChildren<QComboBox*>()) {
    if (combo->findData("pt") >= 0 && combo->findData("fr") >= 0) {
      language = combo;
      break;
    }
  }
  assert(language);
  const QStringList labels = {"Status unavailable", "Estado indisponível",
                             "État indisponible", "Estado no disponible"};
  for (int i = 0; i < labels.size(); ++i) {
    language->setCurrentIndex(i);
    assert(badge->text() == labels[i]);
    assert(!start->isEnabled());
    assert(waitUntil([&]() { return visibleSlots(window) == 1; }));
  }
  language->setCurrentIndex(0);
  assert(summary->text().contains("exceeded 10 seconds"));

  assert(QFile::rename(cliPath, cliPath + ".saved"));
  refresh->click();
  assert(waitUntil([&]() {
    return !control<QPushButton>(window, "doctor")->isEnabled();
  }));
  assert(!start->isEnabled() && badge->text() == "Status unavailable");
  assert(details->toPlainText() == QString::fromUtf8(running).trimmed());
  assert(QFile::rename(cliPath + ".saved", cliPath));
  query("good", "Status............. idle\nExecution.......... none\n");
  assert(badge->text() == "Stopped");
  assert(!badge->property("statusUnavailable").toBool());
  assert(start->isEnabled() && visibleSlots(window) == 0);
  assert(control<QPushButton>(window, "saveThermal")->isEnabled());

  const QByteArray unknown =
      "Slot............... opencl-1\nStatus............. identity unavailable\n"
      "Assignment......... uncertain-assignment\n";
  query("good", unknown);
  assert(badge->text() == "Identity unavailable");
  query("error", {});
  assert(badge->text() == "Status unavailable" && !start->isEnabled());
  assert(details->toPlainText() == QString::fromUtf8(unknown).trimmed());
  assert(visibleSlots(window) == 1);
  query("good", unknown);
  assert(badge->text() == "Identity unavailable" && !start->isEnabled());
  query("good", "Status............. idle\nExecution.......... none\n");
  assert(start->isEnabled());

  // Failure without any confirmed snapshot also blocks new work.
  writeFile(modePath, "empty");
  MainWindow firstFailure;
  firstFailure.show();
  stopAutomaticRefresh(firstFailure);
  assert(!control<QPushButton>(firstFailure, "start")->isEnabled());
  assert(waitUntil([&]() {
    return control<QLabel>(firstFailure, "statusBadge")->text() == "Status unavailable";
  }));
  assert(!control<QPushButton>(firstFailure, "start")->isEnabled());
  assert(visibleSlots(firstFailure) == 0);

  writeFile(modePath, "hold");
  writeFile(responsePath, "Status............. idle\nExecution.......... none\n");
  MainWindow firstTimeout;
  firstTimeout.show();
  stopAutomaticRefresh(firstTimeout);
  shortenStatusTimeout(firstTimeout);
  assert(!control<QPushButton>(firstTimeout, "start")->isEnabled());
  assert(waitUntil([&]() {
    return control<QLabel>(firstTimeout, "statusBadge")->text() == "Status unavailable";
  }));
  assert(!control<QPushButton>(firstTimeout, "start")->isEnabled());
  assert(control<QLabel>(firstTimeout, "statusSummary")->text().contains("exceeded 10 seconds"));
  qInfo() << "UiStatusFailureTests passed";
}

#include "MainWindow.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QLabel>
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
    if (widget->property("controlId").toString() == id) return widget;
  }
  return nullptr;
}

bool waitUntil(const std::function<bool()>& condition) {
  QElapsedTimer timer;
  timer.start();
  do {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    if (condition()) return true;
    QThread::msleep(5);
  } while (timer.elapsed() < 5000);
  return condition();
}

void writeFile(const QString& path, const QByteArray& data) {
  QFile file(path);
  assert(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  assert(file.write(data) == data.size());
}

QByteArray readFile(const QString& path) {
  QFile file(path);
  assert(file.open(QIODevice::ReadOnly));
  return file.readAll();
}

bool statusRunning(MainWindow& window) {
  for (auto* process : window.findChildren<QProcess*>()) {
    if (process->arguments() == QStringList{"status"} &&
        process->state() != QProcess::NotRunning) return true;
  }
  return false;
}

void assertBusy(MainWindow& window) {
  for (const char* id : {"start", "safeStop", "stop", "refresh", "benchmark",
                         "selfTest", "doctor", "updates", "audit", "installKangaroo",
                         "saveThermal"}) {
    auto* button = control<QPushButton>(window, id);
    assert(button);
    assert(!button->isEnabled());
  }
  assert(!control<QComboBox>(window, "executionMode")->isEnabled());
  assert(!control<QCheckBox>(window, "thermalEnabled")->isEnabled());
}
} // namespace

int main(int argc, char* argv[]) {
  QApplication application(argc, argv);
  QCoreApplication::setApplicationName("OpenPuzzleUiCommandBusyTests");
  QCoreApplication::setOrganizationName("OpenPuzzle Project Tests");
  QTemporaryDir isolated;
  assert(isolated.isValid());
  qputenv("HOME", isolated.path().toUtf8());
  qputenv("XDG_CONFIG_HOME", isolated.filePath("config").toUtf8());
  qputenv("XDG_DATA_HOME", isolated.filePath("data").toUtf8());
  QSettings().clear();
  const auto cli = isolated.filePath("openpuzzle");
  const auto calls = isolated.filePath("calls");
  const auto response = isolated.filePath("response");
  const auto statusMode = isolated.filePath("status-mode");
  const auto commandMode = isolated.filePath("command-mode");
  const auto hold = isolated.filePath("hold-command");
  const auto statusHold = isolated.filePath("hold-status");
  const QByteArray idle = "Status............. idle\nExecution.......... none\n";
  writeFile(calls, {});
  writeFile(response, idle);
  writeFile(statusMode, "good\n");
  writeFile(commandMode, "good\n");
  writeFile(cli,
      "#!/bin/sh\n"
      "printf '%s\\n' \"$*\" >> \"$UI_BUSY_CALLS\"\n"
      "if [ \"$1\" = status ]; then\n"
      " mode=$(/bin/cat \"$UI_BUSY_STATUS_MODE\")\n"
      " [ \"$mode\" = error ] && exit 7\n"
      " /bin/cat \"$UI_BUSY_RESPONSE\"\n"
      " while [ -f \"$UI_BUSY_STATUS_HOLD\" ]; do /bin/sleep 0.01; done\n"
      " exit 0\n"
      "fi\n"
      "while [ -f \"$UI_BUSY_HOLD\" ]; do /bin/sleep 0.01; done\n"
      "mode=$(/bin/cat \"$UI_BUSY_COMMAND_MODE\")\n"
      "printf 'finished %s\\n' \"$1\" >> \"$UI_BUSY_CALLS\"\n"
      "[ \"$mode\" = crash ] && kill -TERM \"$$\"\n"
      "[ \"$mode\" = error ] && exit 7\n"
      "exit 0\n");
  assert(QFile::setPermissions(cli,
      QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
  qputenv("OPENPUZZLE_CLI", cli.toUtf8());
  qputenv("UI_BUSY_CALLS", calls.toUtf8());
  qputenv("UI_BUSY_RESPONSE", response.toUtf8());
  qputenv("UI_BUSY_STATUS_MODE", statusMode.toUtf8());
  qputenv("UI_BUSY_COMMAND_MODE", commandMode.toUtf8());
  qputenv("UI_BUSY_HOLD", hold.toUtf8());
  qputenv("UI_BUSY_STATUS_HOLD", statusHold.toUtf8());
  qputenv("OPENPUZZLE_UI_RUNTIME_LOG", isolated.filePath("runtime.log").toUtf8());
  MainWindow window;
  window.show();
  QTimer* pollTimer = nullptr;
  QTimer* statusTimer = nullptr;
  for (auto* timer : window.findChildren<QTimer*>()) {
    if (!timer->isSingleShot() && timer->interval() == 3000) {
      pollTimer = timer;
      pollTimer->stop();
    }
    if (timer->isSingleShot() && timer->interval() == 10000) statusTimer = timer;
  }
  assert(pollTimer && statusTimer);
  auto* start = control<QPushButton>(window, "start");
  auto* doctor = control<QPushButton>(window, "doctor");
  auto* badge = control<QLabel>(window, "statusBadge");
  assert(waitUntil([&] { return start->isEnabled(); }));
  const auto poll = [&] {
    assert(waitUntil([&] { return !statusRunning(window); }));
    const int before = readFile(calls).split('\n').count("status");
    assert(QMetaObject::invokeMethod(pollTimer, "timeout", Qt::DirectConnection));
    assert(waitUntil([&] {
      return readFile(calls).split('\n').count("status") > before && !statusRunning(window);
    }));
  };
  const auto beginDoctor = [&] {
    assert(doctor->isEnabled());
    const int before = readFile(calls).split('\n').count("doctor");
    writeFile(hold, "hold\n");
    doctor->click();
    assert(waitUntil([&] {
      return readFile(calls).split('\n').count("doctor") == before + 1;
    }));
    assertBusy(window);
  };
  const auto finishDoctor = [&] {
    const int before = readFile(calls).split('\n').count("finished doctor");
    assert(QFile::remove(hold));
    assert(waitUntil([&] {
      return readFile(calls).split('\n').count("finished doctor") == before + 1 && doctor->isEnabled();
    }));
  };

  beginDoctor();
  poll();
  qInfo() << "Start enabled during held doctor after idle status:" << start->isEnabled();
  assertBusy(window);
  const auto beforeDisabledClicks = readFile(calls);
  start->click(); doctor->click(); control<QPushButton>(window, "benchmark")->click();
  QCoreApplication::processEvents();
  assert(readFile(calls) == beforeDisabledClicks);

  // Failed queries and recovery must leave the foreground command reserved.
  writeFile(statusMode, "error\n"); poll();
  assert(badge->text() == "Status unavailable"); assertBusy(window);
  writeFile(statusMode, "good\n"); poll();
  assert(badge->text() == "Stopped"); assertBusy(window);
  statusTimer->setInterval(75);
  writeFile(statusHold, "hold\n"); poll();
  assert(badge->text() == "Status unavailable"); assertBusy(window);
  assert(!readFile(calls).contains("finished doctor"));
  statusTimer->setInterval(10000);
  assert(QFile::remove(statusHold)); poll(); assertBusy(window);

  // A status subprocess that cannot start must not release the held doctor.
  qputenv("OPENPUZZLE_CLI", isolated.filePath("missing-cli").toUtf8());
  assert(QMetaObject::invokeMethod(pollTimer, "timeout", Qt::DirectConnection));
  assert(waitUntil([&] { return badge->text() == "Status unavailable" && !statusRunning(window); }));
  assertBusy(window);
  qputenv("OPENPUZZLE_CLI", cli.toUtf8()); poll(); assertBusy(window);

  QComboBox* language = nullptr;
  for (auto* combo : window.findChildren<QComboBox*>()) {
    if (combo->count() == 4 && combo->itemText(0) == "English") language = combo;
  }
  assert(language);
  for (int index = 0; index < 4; ++index) {
    language->setCurrentIndex(index);
    assertBusy(window);
    poll(); assertBusy(window);
  }
  language->setCurrentIndex(0);

  writeFile(response, "Status............. running\nRuntime PID........ 12345\n"); poll();
  assert(badge->text() == "Running"); assertBusy(window);
  finishDoctor();
  assert(!start->isEnabled());
  assert(control<QPushButton>(window, "stop")->isEnabled());
  writeFile(response, idle); poll();
  assert(start->isEnabled());

  // Normal, failed and crashed foreground commands release their own reservation.
  for (const auto& mode : {QByteArray("good\n"), QByteArray("error\n"), QByteArray("crash\n")}) {
    writeFile(commandMode, mode);
    beginDoctor(); poll(); assertBusy(window);
    finishDoctor();
    assert(start->isEnabled());
  }

  // FailedToStart has no finished signal and must also release the reservation.
  assert(QFile::rename(cli, cli + ".saved"));
  doctor->click();
  assert(waitUntil([&] { return doctor->isEnabled(); }));
  assert(QFile::rename(cli + ".saved", cli));
  poll(); assert(start->isEnabled());
  qInfo() << "UiCommandBusyTests passed";
}

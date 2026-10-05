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
#include <QPlainTextEdit>
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

QProcess* statusProcess(MainWindow& window) {
  for (auto* process : window.findChildren<QProcess*>()) {
    if (process->arguments() == QStringList{"status"}) return process;
  }
  return nullptr;
}

void assertReserved(MainWindow& window) {
  for (const char* id : {"start", "safeStop", "stop", "refresh", "benchmark",
                         "selfTest", "doctor", "updates", "audit", "installKangaroo",
                         "saveThermal"}) {
    auto* button = control<QPushButton>(window, id);
    assert(button && !button->isEnabled());
  }
  assert(!control<QComboBox>(window, "executionMode")->isEnabled());
  assert(!control<QCheckBox>(window, "thermalEnabled")->isEnabled());
}
} // namespace

int main(int argc, char* argv[]) {
  QApplication application(argc, argv);
  QCoreApplication::setApplicationName("OpenPuzzleUiLaunchStatusTests");
  QCoreApplication::setOrganizationName("OpenPuzzle Project Tests");
  const QByteArray idle = "Status............. idle\nExecution.......... none\n";
  const QByteArray running = "Status............. running\nRuntime PID........ 12345\n";
  const QByteArray unknown = "Status............. identity unavailable\nRuntime PID........ 12345\n";

  // Every launch uses a bounded fake CLI that records arguments and exits.
  // No bundled engine or real assignment is involved.
  for (const auto& mode : {QByteArray("good"), QByteArray("error"), QByteArray("crash"),
                           QByteArray("empty"), QByteArray("invalid"),
                           QByteArray("read-error"), QByteArray("timeout")}) {
    QTemporaryDir isolated;
    assert(isolated.isValid());
    qputenv("HOME", isolated.path().toUtf8());
    qputenv("XDG_CONFIG_HOME", isolated.filePath("config").toUtf8());
    qputenv("XDG_DATA_HOME", isolated.filePath("data").toUtf8());
    QSettings().clear();
    const auto cli = isolated.filePath("openpuzzle");
    const auto calls = isolated.filePath("calls");
    const auto response = isolated.filePath("response");
    const auto statusMode = isolated.filePath("mode");
    const auto hold = isolated.filePath("hold");
    const auto captured = isolated.filePath("captured");
    writeFile(calls, {});
    writeFile(response, idle);
    writeFile(statusMode, "good");
    writeFile(cli,
        "#!/bin/sh\n"
        "printf '%s\\n' \"$*\" >> \"$UI_LAUNCH_CALLS\"\n"
        "[ \"$1\" = status ] || exit 0\n"
        "mode=$(/bin/cat \"$UI_LAUNCH_MODE\")\n"
        "snapshot=$(/bin/cat \"$UI_LAUNCH_RESPONSE\")\n"
        "printf 'captured\\n' > \"$UI_LAUNCH_CAPTURED\"\n"
        "attempt=0\n"
        "while [ -f \"$UI_LAUNCH_HOLD\" ] && [ \"$attempt\" -lt 500 ]; do\n"
        " /bin/sleep 0.01; attempt=$((attempt + 1))\n"
        "done\n"
        "[ \"$mode\" = error ] && exit 7\n"
        "[ \"$mode\" = crash ] && kill -TERM \"$$\"\n"
        "[ \"$mode\" = empty ] && exit 0\n"
        "[ \"$mode\" = invalid ] && { printf 'unrecognized\\n'; exit 0; }\n"
        "printf '%s\\n' \"$snapshot\"\n");
    assert(QFile::setPermissions(cli,
        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    qputenv("OPENPUZZLE_CLI", cli.toUtf8());
    qputenv("UI_LAUNCH_CALLS", calls.toUtf8());
    qputenv("UI_LAUNCH_RESPONSE", response.toUtf8());
    qputenv("UI_LAUNCH_MODE", statusMode.toUtf8());
    qputenv("UI_LAUNCH_CAPTURED", captured.toUtf8());
    qputenv("UI_LAUNCH_HOLD", hold.toUtf8());
    qputenv("OPENPUZZLE_UI_RUNTIME_LOG", isolated.filePath("runtime.log").toUtf8());
    MainWindow window;
    window.show();
    QTimer* pollTimer = nullptr;
    QTimer* timeoutTimer = nullptr;
    for (auto* timer : window.findChildren<QTimer*>()) {
      if (!timer->isSingleShot() && timer->interval() == 3000) {
        pollTimer = timer;
        pollTimer->stop();
      }
      if (timer->isSingleShot() && timer->interval() == 10000) timeoutTimer = timer;
    }
    assert(pollTimer && timeoutTimer);
    auto* start = control<QPushButton>(window, "start");
    auto* badge = control<QLabel>(window, "statusBadge");
    auto* details = control<QPlainTextEdit>(window, "statusOutput");
    assert(details);
    assert(waitUntil([&] { return start->isEnabled(); }));
    auto* query = statusProcess(window);
    assert(query);
    const auto confirmedDetails = details->toPlainText();
    assert(QFile::remove(captured));
    writeFile(statusMode, mode);
    writeFile(hold, "hold");
    assert(QMetaObject::invokeMethod(pollTimer, "timeout", Qt::DirectConnection));
    assert(waitUntil([&] { return QFile::exists(captured); }));
    assert(query->state() != QProcess::NotRunning);
    assert(start->isEnabled());
    start->click();
    assert(waitUntil([&] { return readFile(calls).contains("run "); }));
    assertReserved(window);

    // A query snapshot taken before Start must never settle the new launch.
    writeFile(response, running);
    writeFile(statusMode, "good");
    if (mode == "read-error") {
      assert(QMetaObject::invokeMethod(query, "errorOccurred", Qt::DirectConnection,
                                     Q_ARG(QProcess::ProcessError, QProcess::ReadError)));
      assertReserved(window);
      assert(badge->text() == "Stopped");
    }
    if (mode == "timeout") {
      assert(QMetaObject::invokeMethod(timeoutTimer, "timeout", Qt::DirectConnection));
      assertReserved(window);
    }
    assert(QFile::remove(hold));
    assert(waitUntil([&] { return query->state() == QProcess::NotRunning; }));
    qInfo() << "Start enabled after pre-launch" << mode << "reply:" << start->isEnabled();
    assertReserved(window);
    assert(badge->text() == "Stopped");
    assert(details->toPlainText() == confirmedDetails);
    const auto recorded = readFile(calls);
    start->click();
    control<QPushButton>(window, "doctor")->click();
    QCoreApplication::processEvents();
    assert(readFile(calls) == recorded);

    QComboBox* language = nullptr;
    for (auto* combo : window.findChildren<QComboBox*>()) {
      if (combo->count() == 4 && combo->itemText(0) == "English") language = combo;
    }
    assert(language);
    for (int index = 0; index < 4; ++index) {
      language->setCurrentIndex(index);
      assertReserved(window);
    }
    language->setCurrentIndex(0);
    const auto poll = [&] {
      assert(waitUntil([&] { return query->state() == QProcess::NotRunning; }));
      const int before = readFile(calls).split('\n').count("status");
      assert(QMetaObject::invokeMethod(pollTimer, "timeout", Qt::DirectConnection));
      assert(waitUntil([&] {
        return readFile(calls).split('\n').count("status") > before &&
               query->state() == QProcess::NotRunning;
      }));
    };
    if (mode == "good") {
      // A normal timer refresh must recover without an extra user click.
      assert(waitUntil([&] { return badge->text() == "Running"; }));
    } else {
      poll();
    }
    assert(badge->text() == "Running");
    assert(!start->isEnabled());
    assert(control<QPushButton>(window, "stop")->isEnabled());
    writeFile(response, unknown); poll();
    assert(!start->isEnabled());
    assert(badge->text() == "Identity unavailable");
    writeFile(response, idle); poll();
    assert(start->isEnabled());

    // Replies from queries begun after a launch still settle its reservation,
    // including a fresh idle response or a fresh failure and later recovery.
    start->click(); assertReserved(window);
    poll(); assert(start->isEnabled());
    start->click(); assertReserved(window);
    writeFile(statusMode, "error"); poll();
    assert(badge->text() == "Status unavailable");
    assert(!start->isEnabled());
    assert(control<QPushButton>(window, "doctor")->isEnabled());
    writeFile(statusMode, "good"); poll();
    assert(start->isEnabled());
  }
  qInfo() << "UiLaunchStatusTests passed";
}

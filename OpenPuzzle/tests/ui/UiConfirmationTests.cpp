#include "MainWindow.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QLabel>
#include <QMessageBox>
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

bool anyProcessRunning(MainWindow& window) {
  for (auto* process : window.findChildren<QProcess*>()) {
    if (process->state() != QProcess::NotRunning) return true;
  }
  return false;
}

struct Scenario {
  const char* button;
  const char* transition;
  bool invoke;
  bool recoverIdle = false;
  bool cancel = false;
};
} // namespace

int main(int argc, char* argv[]) {
  QApplication application(argc, argv);
  QCoreApplication::setApplicationName("OpenPuzzleUiConfirmationTests");
  QCoreApplication::setOrganizationName("OpenPuzzle Project Tests");
  const QByteArray idle = "Status............. idle\nExecution.......... none\n";
  const QByteArray running = "Status............. running\nRuntime PID........ 12345\n";
  const QByteArray unknown = "Status............. identity unavailable\nRuntime PID........ 12345\n";
  const Scenario scenarios[] = {
      {"installKangaroo", "running", false},
      {"installKangaroo", "unknown", false},
      {"installKangaroo", "error", false},
      {"installKangaroo", "invalid", false},
      {"installKangaroo", "timeout", false},
      {"installKangaroo", "running", true, true},
      {"installKangaroo", "idle", true},
      {"installKangaroo", "idle", false, false, true},
      {"stop", "idle", false},
      {"stop", "running", true},
      {"stop", "unknown", true},
      {"stop", "error", true},
      {"stop", "running", false, false, true},
  };
  int scenarioIndex = 0;
  for (const auto& scenario : scenarios) {
    QTemporaryDir isolated;
    assert(isolated.isValid());
    qputenv("HOME", isolated.path().toUtf8());
    qputenv("XDG_CONFIG_HOME", isolated.filePath("config").toUtf8());
    qputenv("XDG_DATA_HOME", isolated.filePath("data").toUtf8());
    QSettings().clear();
    const auto cli = isolated.filePath("openpuzzle");
    const auto calls = isolated.filePath("calls");
    const auto response = isolated.filePath("response");
    const auto mode = isolated.filePath("mode");
    const auto hold = isolated.filePath("hold");
    const auto captured = isolated.filePath("captured");
    const bool installing = QByteArray(scenario.button) == "installKangaroo";
    const QByteArray command = installing ? "engine install psckangaroo" : "stop";
    writeFile(calls, {});
    writeFile(response, installing ? idle : running);
    writeFile(mode, "good");
    writeFile(cli,
        "#!/bin/sh\n"
        "printf '%s\\n' \"$*\" >> \"$UI_CONFIRM_CALLS\"\n"
        "[ \"$1\" = status ] || exit 0\n"
        "mode=$(/bin/cat \"$UI_CONFIRM_MODE\")\n"
        "printf 'captured\\n' > \"$UI_CONFIRM_CAPTURED\"\n"
        "attempt=0\n"
        "while [ -f \"$UI_CONFIRM_HOLD\" ] && [ \"$attempt\" -lt 500 ]; do\n"
        " /bin/sleep 0.01; attempt=$((attempt + 1))\n"
        "done\n"
        "[ \"$mode\" = error ] && exit 7\n"
        "[ \"$mode\" = invalid ] && { printf 'unrecognized\\n'; exit 0; }\n"
        "exec /bin/cat \"$UI_CONFIRM_RESPONSE\"\n");
    assert(QFile::setPermissions(cli,
        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    qputenv("OPENPUZZLE_CLI", cli.toUtf8());
    qputenv("UI_CONFIRM_CALLS", calls.toUtf8());
    qputenv("UI_CONFIRM_RESPONSE", response.toUtf8());
    qputenv("UI_CONFIRM_MODE", mode.toUtf8());
    qputenv("UI_CONFIRM_HOLD", hold.toUtf8());
    qputenv("UI_CONFIRM_CAPTURED", captured.toUtf8());
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
    auto* button = control<QPushButton>(window, scenario.button);
    auto* badge = control<QLabel>(window, "statusBadge");
    assert(waitUntil([&] { return button->isEnabled() && !statusRunning(window); }));
    assert(QFile::remove(captured));
    const QByteArray transition(scenario.transition);
    const QString expectedBadge = transition == "running" ? "Running"
        : (transition == "unknown" ? "Identity unavailable"
        : (transition == "idle" ? "Stopped" : "Status unavailable"));
    QTimer watcher;
    QTimer deadline;
    deadline.setSingleShot(true);
    QObject::connect(&deadline, &QTimer::timeout, [] { assert(false && "Confirmation fixture timed out"); });
    int phase = 0;
    bool confirmedEnabled = false;
    bool timeoutTriggered = false;
    QMessageBox* dialog = nullptr;
    QObject::connect(&watcher, &QTimer::timeout, [&] {
      if (!dialog) {
        for (auto* widget : QApplication::topLevelWidgets()) {
          auto* candidate = qobject_cast<QMessageBox*>(widget);
          if (candidate && candidate->isVisible()) dialog = candidate;
        }
      }
      if (!dialog) return;
      if (phase == 0) {
        writeFile(response, transition == "running" ? running : transition == "unknown" ? unknown : idle);
        writeFile(mode, transition == "error" ? "error" : transition == "invalid" ? "invalid" : "good");
        if (transition == "timeout") writeFile(hold, "hold");
        assert(QMetaObject::invokeMethod(pollTimer, "timeout", Qt::DirectConnection));
        phase = 1;
        return;
      }
      if (phase == 1 && transition == "timeout" && !timeoutTriggered && QFile::exists(captured) && statusRunning(window)) {
        timeoutTriggered = true;
        assert(QMetaObject::invokeMethod(timeoutTimer, "timeout", Qt::DirectConnection));
        assert(QFile::remove(hold));
      }
      if (statusRunning(window)) return;
      if (phase == 1 && badge->text() != expectedBadge) return;
      if (phase == 1 && scenario.recoverIdle) {
        assert(!button->isEnabled());
        writeFile(response, idle);
        assert(QMetaObject::invokeMethod(pollTimer, "timeout", Qt::DirectConnection));
        phase = 2;
        return;
      }
      if (phase == 2 && badge->text() != "Stopped") return;
      confirmedEnabled = button->isEnabled();
      QComboBox* language = nullptr;
      for (auto* combo : window.findChildren<QComboBox*>()) {
        if (combo->count() == 4 && combo->itemText(0) == "English") language = combo;
      }
      assert(language);
      language->setCurrentIndex(scenarioIndex % 4);
      assert(button->isEnabled() == confirmedEnabled);
      auto* answer = dialog->button(scenario.cancel ? QMessageBox::No : QMessageBox::Yes);
      assert(answer);
      watcher.stop();
      answer->click();
    });
    watcher.start(5);
    deadline.start(5000);
    button->click(); // Nested confirmation loop receives actual status completions.
    watcher.stop();
    deadline.stop();
    assert(dialog && phase > 0);
    assert(waitUntil([&] { return !anyProcessRunning(window); }));
    const int invocations = readFile(calls).split('\n').count(command);
    qInfo() << scenario.button << scenario.transition << "enabled at confirmation:" << confirmedEnabled
            << "CLI invocations:" << invocations;
    assert(invocations == (scenario.invoke ? 1 : 0));
    if (!scenario.cancel) assert(confirmedEnabled == scenario.invoke);
    ++scenarioIndex;
  }
  qInfo() << "UiConfirmationTests passed";
}

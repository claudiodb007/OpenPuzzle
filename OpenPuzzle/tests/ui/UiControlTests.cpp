#include "MainWindow.hpp"

#include "openpuzzle/ui/RunCommandBuilder.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <QWidget>

#include <cassert>
#include <functional>

using openpuzzle::ui::MainWindow;
using openpuzzle::ui::RunMode;

namespace {

template<typename Widget>
Widget* control(MainWindow& window, const char* id) {
  for (auto* candidate : window.findChildren<Widget*>()) {
    if (candidate->property("controlId").toString() ==
        QString::fromLatin1(id)) {
      return candidate;
    }
  }

  return nullptr;
}

QString logContents(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return {};
  }

  return QString::fromUtf8(file.readAll());
}

bool waitUntil(const std::function<bool()>& condition) {
  QElapsedTimer timer;
  timer.start();

  while (timer.elapsed() < 5000) {
    QCoreApplication::processEvents(
        QEventLoop::AllEvents,
        50);
    if (condition()) {
      return true;
    }
    QThread::msleep(10);
  }

  return condition();
}

void clickAndExpect(
    QPushButton* button,
    const QString& logPath,
    const QString& expected) {
  assert(button != nullptr);
  assert(waitUntil([button]() {
    return button->isEnabled();
  }));

  button->click();
  assert(waitUntil([&]() {
    return logContents(logPath).contains(expected);
  }));
}

} // namespace

int main(int argc, char* argv[]) {
  QApplication application(argc, argv);
  QCoreApplication::setApplicationName("OpenPuzzleUiControlTests");
  QCoreApplication::setOrganizationName("OpenPuzzle Project Tests");

  QTemporaryDir temporary;
  assert(temporary.isValid());

  const QString isolatedConfig = temporary.filePath("config");
  assert(QDir().mkpath(isolatedConfig));
  qputenv("XDG_CONFIG_HOME", isolatedConfig.toUtf8());

  const QString cliPath = temporary.filePath("openpuzzle");
  const QString systemctlPath = temporary.filePath("systemctl");
  const QString logPath = temporary.filePath("commands.log");
  const QString statusPath = temporary.filePath("status.txt");
  const QString runtimeLogPath =
      temporary.filePath("ui-runtime.log");

  QFile statusFile(statusPath);
  assert(statusFile.open(QIODevice::WriteOnly | QIODevice::Text));
  statusFile.write(
      "OpenPuzzle Status\n"
      "-----------------\n"
      "Status............. idle\n"
      "Execution.......... none\n");
  statusFile.close();

  QFile cli(cliPath);
  assert(cli.open(QIODevice::WriteOnly | QIODevice::Text));
  QTextStream script(&cli);
  script
      << "#!/usr/bin/env bash\n"
      << "printf '%s\\n' \"$*\" >> \"$OPENPUZZLE_TEST_LOG\"\n"
      << "if [[ \"${1:-}\" == run ]]; then\n"
      << "  printf '%s\\n' 'continuous-runtime-output'\n"
      << "fi\n"
      << "if [[ \"${1:-}\" == status ]]; then\n"
      << "  cat \"$OPENPUZZLE_TEST_STATUS\"\n"
      << "fi\n";
  script.flush();
  cli.close();
  assert(cli.setPermissions(
      QFileDevice::ReadOwner |
      QFileDevice::WriteOwner |
      QFileDevice::ExeOwner));

  QFile systemctl(systemctlPath);
  assert(systemctl.open(QIODevice::WriteOnly | QIODevice::Text));
  QTextStream systemctlScript(&systemctl);
  systemctlScript
      << "#!/usr/bin/env bash\n"
      << "printf 'systemctl %s\\n' \"$*\" >> \"$OPENPUZZLE_TEST_LOG\"\n";
  systemctlScript.flush();
  systemctl.close();
  assert(systemctl.setPermissions(
      QFileDevice::ReadOwner |
      QFileDevice::WriteOwner |
      QFileDevice::ExeOwner));

  qputenv("OPENPUZZLE_CLI", cliPath.toUtf8());
  qputenv("OPENPUZZLE_TEST_LOG", logPath.toUtf8());
  qputenv("OPENPUZZLE_TEST_STATUS", statusPath.toUtf8());
  qputenv(
      "OPENPUZZLE_UI_RUNTIME_LOG",
      runtimeLogPath.toUtf8());
  qputenv("OPENPUZZLE_SYSTEMCTL", systemctlPath.toUtf8());
  QSettings().clear();

  MainWindow window;
  window.show();

  auto* statusBadge = control<QLabel>(window, "statusBadge");
  auto* autoStart = control<QCheckBox>(window, "autoStart");
  assert(statusBadge != nullptr);
  assert(autoStart != nullptr && !autoStart->isChecked());

  autoStart->click();
  assert(autoStart->isChecked());
  assert(logContents(logPath).contains(
      "systemctl --user daemon-reload"));
  assert(logContents(logPath).contains(
      "systemctl --user enable openpuzzle-ui-autostart.service"));

  QFile automaticUnit(
      QStandardPaths::writableLocation(
          QStandardPaths::ConfigLocation) +
      "/systemd/user/openpuzzle-ui-autostart.service");
  assert(automaticUnit.open(QIODevice::ReadOnly | QIODevice::Text));
  const QString automaticUnitContents =
      QString::fromUtf8(automaticUnit.readAll());
  automaticUnit.close();
  assert(automaticUnitContents.contains(
      "openpuzzle\" \"run\" \"71\""));
  assert(!automaticUnitContents.contains("--now"));

  autoStart->click();
  assert(!autoStart->isChecked());
  assert(logContents(logPath).contains(
      "systemctl --user disable openpuzzle-ui-autostart.service"));
  assert(!QFile::exists(automaticUnit.fileName()));
  assert(statusBadge->text() == "Stopped");
  assert(!statusBadge->property("active").toBool());

  for (auto* label : window.findChildren<QLabel*>()) {
    assert(label->text() != "OpenPuzzle");
  }

  assert(waitUntil([&]() {
    return logContents(logPath).contains("status");
  }));

  auto* start = control<QPushButton>(window, "start");
  auto* safeStop = control<QPushButton>(window, "safeStop");
  auto* stop = control<QPushButton>(window, "stop");
  auto* installKangaroo =
      control<QPushButton>(window, "installKangaroo");

  assert(start != nullptr);
  assert(safeStop != nullptr && !safeStop->isEnabled());
  assert(stop != nullptr && !stop->isEnabled());
  assert(installKangaroo != nullptr);
  assert(installKangaroo->isEnabled());

  assert(statusFile.open(
      QIODevice::WriteOnly |
      QIODevice::Truncate |
      QIODevice::Text));
  statusFile.write(
      "OpenPuzzle Status\n"
      "-----------------\n\n"
      "Slot............... cuda\n"
      "Status............. waiting\n"
      "Execution.......... none\n\n"
      "Slot............... opencl\n"
      "Status............. waiting\n"
      "Execution.......... none\n");
  statusFile.close();

  auto* refresh = control<QPushButton>(window, "refresh");
  assert(refresh != nullptr);
  refresh->click();
  assert(waitUntil([statusBadge]() {
    return statusBadge->text() == "Running";
  }));
  assert(statusBadge->property("active").toBool());

  assert(statusFile.open(
      QIODevice::WriteOnly |
      QIODevice::Truncate |
      QIODevice::Text));
  statusFile.write(
      "OpenPuzzle Status\n"
      "-----------------\n"
      "Status............. idle\n"
      "Execution.......... none\n");
  statusFile.close();
  refresh->click();
  assert(waitUntil([statusBadge]() {
    return statusBadge->text() == "Stopped";
  }));

  clickAndExpect(
      control<QPushButton>(window, "benchmark"),
      logPath,
      "benchmark");

  auto* messagesOutput =
      control<QPlainTextEdit>(window, "messagesOutput");
  assert(messagesOutput != nullptr);
  assert(waitUntil([messagesOutput]() {
    return messagesOutput->toPlainText().contains("Benchmark");
  }));
  clickAndExpect(
      control<QPushButton>(window, "selfTest"),
      logPath,
      "selftest --backend cuda --device 0");
  clickAndExpect(
      control<QPushButton>(window, "doctor"),
      logPath,
      "doctor");
  clickAndExpect(
      control<QPushButton>(window, "updates"),
      logPath,
      "update --check");
  clickAndExpect(
      control<QPushButton>(window, "audit"),
      logPath,
      "audit --limit 20");

  auto* mode = control<QComboBox>(window, "executionMode");
  auto* rusticl = control<QCheckBox>(window, "rusticlRadeonsi");
  auto* openclDevice = control<QWidget>(window, "openclDevice");
  auto* kangarooPuzzle =
      control<QComboBox>(window, "kangarooPuzzle");

  assert(mode != nullptr);
  assert(mode->count() == 5);
  mode->setCurrentIndex(
      mode->findData(
          static_cast<int>(RunMode::BitCrackCudaOpencl)));
  assert(rusticl != nullptr && !rusticl->isHidden());
  assert(openclDevice != nullptr && !openclDevice->isHidden());
  rusticl->setChecked(true);

  clickAndExpect(
      start,
      logPath,
      "run 71 --engine bitcrack --backend cuda --device 0 "
      "--with-opencl --opencl-device 1 --rusticl-enable radeonsi");

  assert(waitUntil([&]() {
    return logContents(runtimeLogPath).contains(
        "continuous-runtime-output");
  }));

  QFileInfo runtimeLogInfo(runtimeLogPath);
  assert(runtimeLogInfo.isFile());
  const auto runtimeLogPermissions =
      runtimeLogInfo.permissions();
  assert(
      (runtimeLogPermissions &
       (QFileDevice::ReadGroup |
        QFileDevice::WriteGroup |
        QFileDevice::ExeGroup |
        QFileDevice::ReadOther |
        QFileDevice::WriteOther |
        QFileDevice::ExeOther)) == 0);

  auto* runtimeLogOutput =
      control<QPlainTextEdit>(
          window, "runtimeLogOutput");
  assert(runtimeLogOutput != nullptr);
  refresh->click();
  assert(waitUntil([runtimeLogOutput]() {
    return runtimeLogOutput->toPlainText().contains(
        "continuous-runtime-output");
  }));

  assert(waitUntil([start]() {
    return start->isEnabled();
  }));

  mode->setCurrentIndex(
      mode->findData(
          static_cast<int>(RunMode::KangarooCuda)));
  assert(kangarooPuzzle != nullptr);
  assert(!kangarooPuzzle->isHidden());
  assert(kangarooPuzzle->count() == 5);
  assert(kangarooPuzzle->itemData(0).toInt() == 140);
  assert(kangarooPuzzle->itemData(4).toInt() == 160);

  const QString messageBeforeStatusRefresh =
      messagesOutput->toPlainText();

  assert(statusFile.open(
      QIODevice::WriteOnly |
      QIODevice::Truncate |
      QIODevice::Text));
  statusFile.write(
      "OpenPuzzle Status\n"
      "-----------------\n\n"
      "Slot............... cuda\n"
      "Status............. running\n"
      "Assignment......... cuda-id\n"
      "Puzzle............. 71\n"
      "Assignment number... 2720\n"
      "Engine............. BitCrack\n"
      "Backend............ CUDA\n"
      "Speed.............. 1487.41 MKey/s\n"
      "Progress........... uploaded\n\n"
      "Slot............... opencl\n"
      "Status............. running\n"
      "Assignment......... opencl-id\n"
      "Puzzle............. 71\n"
      "Assignment number... 2719\n"
      "Engine............. BitCrack\n"
      "Backend............ OpenCL\n"
      "Speed.............. 203.58 MKey/s\n"
      "Progress........... uploaded\n");
  statusFile.close();

  refresh->click();

  assert(waitUntil([&window]() {
    int slotCount = 0;
    for (auto* widget : window.findChildren<QWidget*>()) {
      if (widget->property("controlId").toString() ==
          "runtimeSlot") {
        ++slotCount;
      }
    }
    return slotCount == 2;
  }));

  assert(safeStop->isEnabled());
  assert(stop->isEnabled());
  assert(!start->isEnabled());
  assert(statusBadge->text() == "Running");
  assert(statusBadge->property("active").toBool());
  assert(messagesOutput->toPlainText() ==
         messageBeforeStatusRefresh);

  refresh->click();
  assert(statusBadge->text() == "Running");

  auto* statusOutput =
      control<QPlainTextEdit>(window, "statusOutput");
  assert(statusOutput != nullptr);
  assert(statusOutput->toPlainText().contains(
      "Slot............... cuda"));
  assert(statusOutput->toPlainText().contains(
      "Slot............... opencl"));

  assert(statusFile.open(
      QIODevice::WriteOnly |
      QIODevice::Truncate |
      QIODevice::Text));
  statusFile.write(
      "OpenPuzzle Status\n"
      "-----------------\n\n"
      "Slot............... cpu\n"
      "Status............. running\n"
      "Assignment......... cpu-id\n"
      "Puzzle............. 71\n"
      "Assignment number... 2721\n"
      "Engine............. KeyHunt\n"
      "Backend............ CPU\n"
      "Speed.............. 4.73 MKey/s\n"
      "Progress........... uploaded\n");
  statusFile.close();
  refresh->click();

  assert(waitUntil([&window]() {
    int slotCount = 0;
    for (auto* widget : window.findChildren<QWidget*>()) {
      if (widget->property("controlId").toString() ==
          "runtimeSlot") {
        ++slotCount;
      }
    }
    return slotCount == 1;
  }));
  assert(statusOutput->toPlainText().contains(
      "Engine............. KeyHunt"));
  assert(statusOutput->toPlainText().contains(
      "Backend............ CPU"));

  assert(statusFile.open(
      QIODevice::WriteOnly |
      QIODevice::Truncate |
      QIODevice::Text));
  statusFile.write(
      "OpenPuzzle Status\n"
      "-----------------\n\n"
      "Slot............... cpu\n"
      "Status............. solution found\n"
      "Assignment......... synthetic-solution-id\n"
      "Puzzle............. 71\n"
      "Engine............. KeyHunt\n"
      "Backend............ CPU\n"
      "Solution file...... /private/workspace/found.txt\n"
      "Private key........ not displayed\n");
  statusFile.close();
  refresh->click();

  assert(waitUntil([messagesOutput]() {
    return messagesOutput->toPlainText().contains(
        "Solution found");
  }));
  assert(messagesOutput->toPlainText().contains(
      "~/OpenPuzzle-Solutions"));
  assert(messagesOutput->toPlainText().contains(
      "not displayed or uploaded"));
  assert(!messagesOutput->toPlainText().contains(
      "/private/workspace/found.txt"));
  assert(statusBadge->text() == "Stopped");
  assert(!statusBadge->property("active").toBool());
  assert(!start->isEnabled());

  return 0;
}

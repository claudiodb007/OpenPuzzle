#include "MainWindow.hpp"
#include "GpuControlPanel.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <cassert>
#include <functional>
#include <iostream>
using openpuzzle::ui::MainWindow;
namespace {
template<class T> T* control(MainWindow& window, const char* name) {
  for (auto* widget : window.findChildren<T*>())
    if (widget->property("controlId").toString() == name) return widget;
  assert(false); return nullptr;
}
bool until(const std::function<bool()>& condition) {
  QElapsedTimer elapsed; elapsed.start();
  do { QApplication::processEvents(); if (condition()) return true; QThread::msleep(10); }
  while (elapsed.elapsed() < 7000);
  return condition();
}
void write(const QString& path, const QByteArray& data) {
  QFile file(path); assert(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  assert(file.write(data) == data.size());
}
QByteArray read(const QString& path) {
  QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {}; return file.readAll();
}
}
int main(int argc, char** argv) {
  QApplication app(argc, argv);
  app.setApplicationName("OpenPuzzleUiBenchmarkTests");
  app.setOrganizationName("OpenPuzzle Tests");
  QTemporaryDir temp; assert(temp.isValid());
  qputenv("HOME", temp.path().toUtf8());
  qputenv("XDG_CONFIG_HOME", temp.filePath("config").toUtf8());
  qputenv("XDG_DATA_HOME", temp.filePath("data").toUtf8());
  qputenv("OPENPUZZLE_TEST_ROOT", temp.path().toUtf8());
  const auto cli = temp.filePath("synthetic cli ' ç");
  write(cli, R"SH(#!/bin/bash
case "$1" in
 status) printf 'Status............. idle\nExecution.......... none\n';;
 benchmark)
   printf '%s\n' "$@" > "$OPENPUZZLE_TEST_ROOT/argv"
   trap 'echo cancelled; exit 130' TERM
   printf 'measuring synthetic candidate\n'
   printf '\303'; sleep .05; printf '\247\n'
   while test -f "$OPENPUZZLE_TEST_ROOT/hold"; do sleep .05; done
   if test -f "$OPENPUZZLE_TEST_ROOT/fail"; then echo 'synthetic failed'; exit 1; fi
   printf 'validated synthetic profile\n' > "$OPENPUZZLE_TEST_ROOT/profile"
   echo 'Saved profile';;
 stop) echo 'UNEXPECTED STOP' > "$OPENPUZZLE_TEST_ROOT/wrong-stop";;
esac
)SH");
  assert(QFile::setPermissions(cli, QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner));
  qputenv("OPENPUZZLE_CLI", cli.toUtf8());
  QSettings().clear();
  MainWindow window; window.show();
  auto* benchmark = control<QPushButton>(window, "benchmark");
  auto* stop = control<QPushButton>(window, "stop");
  auto* output = control<QPlainTextEdit>(window, "messagesOutput");
  assert(until([&] { return benchmark->isEnabled(); }));
  write(temp.filePath("profile"), "previous profile\n");
  // Declining the dialog must not start a process or mutate the profile.
  QTimer::singleShot(0, [&] {
    auto* dialog = window.findChild<QDialog*>("linuxBenchmarkDialog"); assert(dialog);
    assert(dialog->findChild<QPushButton*>("benchmarkConfirm")); dialog->reject();
  });
  benchmark->click();
  assert(!QFile::exists(temp.filePath("argv")));
  auto launch = [&](bool opencl, int device, bool rusticl) {
    assert(until([&] { return benchmark->isEnabled(); }));
    QTimer::singleShot(0, [&window, opencl, device, rusticl] {
      auto* dialog = window.findChild<QDialog*>("linuxBenchmarkDialog"); assert(dialog);
      dialog->findChild<QComboBox*>("benchmarkBackend")->setCurrentIndex(opencl ? 1 : 0);
      dialog->findChild<QSpinBox*>("benchmarkDevice")->setValue(device);
      dialog->findChild<QCheckBox*>("benchmarkRusticl")->setChecked(rusticl);
      dialog->findChild<QPushButton*>("benchmarkConfirm")->click();
    });
    benchmark->click();
    assert(until([&] { return output->toPlainText().contains("measuring synthetic candidate"); }));
  };
  write(temp.filePath("hold"), "hold");
  launch(false, 2, true);
  assert(read(temp.filePath("argv")) == "benchmark\n--backend\ncuda\n--gpu\n2\n--auto\n--real\n--require-idle\n");
  assert(until([&] { return output->toPlainText().contains(QString::fromUtf8("ç")); }));
  assert(!output->toPlainText().contains(QChar::ReplacementCharacter));
  assert(!benchmark->isEnabled() && stop->isEnabled());
  assert(!control<QPushButton>(window,"start")->isEnabled());
  assert(!control<QPushButton>(window,"safeStop")->isEnabled());
  openpuzzle::ui::GpuControlPanel* gpuPanel = nullptr;
  for (auto* frame : window.findChildren<QFrame*>())
    if (auto* panel = dynamic_cast<openpuzzle::ui::GpuControlPanel*>(frame)) gpuPanel = panel;
  assert(gpuPanel && !gpuPanel->isEnabled());
  QTimer::singleShot(0, [] {
    for (auto* widget : QApplication::topLevelWidgets())
      if (auto* box = qobject_cast<QMessageBox*>(widget)) box->accept();
  });
  window.close(); assert(window.isVisible());
  stop->click(); assert(!stop->isEnabled());
  assert(until([&] { return benchmark->isEnabled(); }));
  assert(output->toPlainText().contains("Previous profile preserved"));
  assert(read(temp.filePath("profile")) == "previous profile\n");
  assert(!QFile::exists(temp.filePath("wrong-stop")));
  assert(gpuPanel->isEnabled());
  QFile::remove(temp.filePath("hold"));
  write(temp.filePath("fail"), "fail");
  launch(true, 3, true);
  assert(until([&] { return benchmark->isEnabled(); }));
  assert(read(temp.filePath("argv")) == "benchmark\n--backend\nopencl\n--gpu\n3\n--auto\n--real\n--require-idle\n--rusticl-enable\nradeonsi\n");
  assert(read(temp.filePath("profile")) == "previous profile\n");
  assert(output->toPlainText().contains("failed"));
  QFile::remove(temp.filePath("fail"));
  launch(true, 1, false);
  assert(until([&] { return benchmark->isEnabled(); }));
  assert(!read(temp.filePath("argv")).contains("rusticl"));
  assert(read(temp.filePath("profile")) == "validated synthetic profile\n");
  assert(output->toPlainText().contains("saved"));
  std::cout << "UiBenchmarkTests passed: confirmation, literal GPU arguments, streamed UTF-8, cancellation, close guard, failed profile preservation and success.\n";
}

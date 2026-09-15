#include "MainWindow.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QTimer>

int main(int argc, char* argv[]) {
  QApplication application(argc, argv);

  QCoreApplication::setApplicationName(
      "OpenPuzzle");
  QCoreApplication::setApplicationVersion(
      OPENPUZZLE_VERSION);
  QCoreApplication::setOrganizationName(
      "OpenPuzzle Project");
  QGuiApplication::setDesktopFileName(
      "openpuzzle-ui");
  QApplication::setWindowIcon(
      QIcon(":/branding/openpuzzle-icon.png"));

  openpuzzle::ui::MainWindow window;
  window.show();

  if (QCoreApplication::arguments().contains("--smoke-test")) {
    QTimer::singleShot(
        750,
        &application,
        &QCoreApplication::quit);
  }

  return application.exec();
}

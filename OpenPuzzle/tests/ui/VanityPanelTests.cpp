#include "VanityPanel.hpp"
#include "UiTheme.hpp"
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <cassert>
#include <functional>
#include <iostream>
using namespace openpuzzle::ui;
bool waitFor(std::function<bool()> f){QElapsedTimer t;t.start();do{QApplication::processEvents();if(f())return true;QThread::msleep(10);}while(t.elapsed()<5000);return f();}
void write(const QString& path,const QByteArray& data){QFile f(path);assert(f.open(QIODevice::WriteOnly));assert(f.write(data)==data.size());}
void yes(){QTimer::singleShot(0,[]{for(auto* w:QApplication::topLevelWidgets())if(auto* m=qobject_cast<QMessageBox*>(w))m->button(QMessageBox::Yes)->click();});}
int main(int argc,char** argv){
 QApplication app(argc,argv);QTemporaryDir tmp;assert(tmp.isValid());auto cli=tmp.filePath("fake cli ç ' $()");
 write(cli,R"SH(#!/bin/bash
printf '%s\n' "$@" > "$OPENPUZZLE_VANITY_STORE/argv"
if [ "$2" = measured-show ]; then
 if [ -f "$OPENPUZZLE_VANITY_STORE/fail" ]; then echo damaged; exit 2; fi
 echo '{"status":"ready","median_mkeys":2597.33,"planning_mkeys":2519.41,"gpu":"Synthetic GPU","grid":1024,"threads":256,"points":1024}'
 exit 0
fi
trap 'echo cancelled; exit 130' TERM
if [ "$2" = measured-test ]; then
 for arg; do root="$arg"; done
 mkdir -p "$root/controls"
 echo '{"status":"running","checked":100,"total":200,"speed_mkeys":123}'
 while [ ! -f "$root/controls/safe-stop" ]; do sleep .02; done
 echo '{"status":"completed"}';exit 0
fi
if [ "$2" = measured-benchmark ]; then
 echo 'synthetic measurement'
 while true;do sleep .02;done
fi
exit 2
)SH");QFile::setPermissions(cli,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
 qputenv("OPENPUZZLE_VANITY_STORE",tmp.path().toUtf8());
 for(int language=0;language<4;++language){
  VanityPanel panel(cli,static_cast<UiLanguage>(language));panel.setStyleSheet(themeStyleSheet(language%2?UiTheme::Dark:UiTheme::Light));panel.show();
  auto* run=panel.findChild<QPushButton*>("vanityRun");auto* stop=panel.findChild<QPushButton*>("vanityStop");auto* safe=panel.findChild<QPushButton*>("vanitySafe");auto* bench=panel.findChild<QPushButton*>("vanityBenchmark");auto* verify=panel.findChild<QPushButton*>("vanityVerify");
  assert(waitFor([&]{return run->isEnabled();}));assert(panel.findChild<QLabel*>("vanityProfile")->text().contains("2519.41"));
  if(language==1&&!qEnvironmentVariableIsEmpty("OPV_PANEL_SCREENSHOT"))panel.grab().save(qEnvironmentVariable("OPV_PANEL_SCREENSHOT"));
  // Declining must not replace last show argv or launch anything.
  QTimer::singleShot(0,[]{for(auto* w:QApplication::topLevelWidgets())if(auto* m=qobject_cast<QMessageBox*>(w))m->button(QMessageBox::No)->click();});run->click();assert(!stop->isEnabled());
  yes();run->click();assert(waitFor([&]{return safe->isEnabled();}));assert(!verify->isEnabled()&&!bench->isEnabled());
  panel.close();assert(panel.isVisible());assert(panel.findChild<QLabel*>("vanityStatus"));safe->click();assert(!safe->isEnabled());assert(waitFor([&]{return run->isEnabled();}));
  yes();bench->click();assert(waitFor([&]{return panel.findChild<QPlainTextEdit*>("vanityOutput")->toPlainText().contains("synthetic measurement");}));assert(!safe->isEnabled());stop->click();assert(!stop->isEnabled());assert(waitFor([&]{return run->isEnabled();}));
  write(tmp.filePath("fail"),"fail");verify->click();assert(waitFor([&]{return verify->isEnabled();}));assert(!run->isEnabled()&&!bench->isEnabled());QFile::remove(tmp.filePath("fail"));verify->click();assert(waitFor([&]{return run->isEnabled();}));panel.close();
 }
 std::cout<<"VanityPanelTests passed: four languages, selection, declined measurement, progress, Safe Stop, cancellation, close guard and failed verification.\n";
}

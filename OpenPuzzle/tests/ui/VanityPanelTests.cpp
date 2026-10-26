#include "VanityPanel.hpp"
#include "UiTheme.hpp"
#include <QApplication>
#include <QComboBox>
#include <QJsonDocument>
#include <QJsonArray>
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
 write(cli,R"SH(#!/usr/bin/env python3
import json,os,sys,time,signal
from pathlib import Path
store=Path(os.environ['OPENPUZZLE_VANITY_STORE']);args=sys.argv[1:];action=args[1]
(store/'argv').write_text('\n'.join(args))
if action=='measured-show':
 if (store/'fail').exists():print('damaged');sys.exit(2)
 print(json.dumps(dict(status='ready',median_mkeys=2597.33,planning_mkeys=2519.41,gpu='Synthetic GPU',grid=1024,threads=256,points=1024)));sys.exit()
if action=='measured-runs':
 rows=[json.loads((store/'saved-run').read_text())] if (store/'saved-run').exists() else []
 print(json.dumps(dict(status='local-runs',runs=rows)));sys.exit()
root=None
if action in ('measured-test','measured-resume'):
 root=Path(args[args.index('--run-root')+1]);(root/'controls').mkdir(parents=True,exist_ok=True)
 (store/'last-execution-argv').write_text(json.dumps(args))
 if action=='measured-resume':
  assert args[args.index('--expected-state')+1]=='a'*64
  if (root/'controls/cancel').exists():(root/'controls/cancel').unlink()
def stop(*_):
 if root:
  (root/'controls/cancel').touch()
  (store/'saved-run').write_text(json.dumps(dict(path=str(root),available=True,checked=100,total=200,state_sha256='a'*64)))
 print('cancelled',flush=True);sys.exit(130)
signal.signal(signal.SIGTERM,stop)
if action in ('measured-test','measured-resume'):
 print(json.dumps(dict(status='running',checked=100,total=200,speed_mkeys=123)),flush=True)
 until=time.monotonic()+.4
 while not (root/'controls/safe-stop').exists():
  if action=='measured-resume' and time.monotonic()>until:break
  time.sleep(.01)
 if (store/'saved-run').exists():(store/'saved-run').unlink()
 print('{"status":"completed"}',flush=True);sys.exit()
if action=='measured-benchmark':
 print('synthetic measurement',flush=True)
 while True:time.sleep(.01)
sys.exit(2)
)SH");QFile::setPermissions(cli,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
 qputenv("OPENPUZZLE_VANITY_STORE",tmp.path().toUtf8());
 for(int language=0;language<4;++language){
  VanityPanel panel(cli,static_cast<UiLanguage>(language));panel.setStyleSheet(themeStyleSheet(language%2?UiTheme::Dark:UiTheme::Light));panel.show();
  auto* run=panel.findChild<QPushButton*>("vanityRun");auto* stop=panel.findChild<QPushButton*>("vanityStop");auto* safe=panel.findChild<QPushButton*>("vanitySafe");auto* bench=panel.findChild<QPushButton*>("vanityBenchmark");auto* verify=panel.findChild<QPushButton*>("vanityVerify");auto* resume=panel.findChild<QPushButton*>("vanityResume");
  assert(waitFor([&]{return run->isEnabled();}));assert(panel.findChild<QLabel*>("vanityProfile")->text().contains("2519.41"));
  if(language==1&&!qEnvironmentVariableIsEmpty("OPV_PANEL_SCREENSHOT"))panel.grab().save(qEnvironmentVariable("OPV_PANEL_SCREENSHOT"));
  // Declining must not replace last show argv or launch anything.
  QTimer::singleShot(0,[]{for(auto* w:QApplication::topLevelWidgets())if(auto* m=qobject_cast<QMessageBox*>(w))m->button(QMessageBox::No)->click();});run->click();assert(!stop->isEnabled());
  yes();run->click();assert(waitFor([&]{return safe->isEnabled();}));assert(!verify->isEnabled()&&!bench->isEnabled());
  panel.close();assert(panel.isVisible());assert(panel.findChild<QLabel*>("vanityStatus"));safe->click();assert(!safe->isEnabled());assert(waitFor([&]{return run->isEnabled();}));
  // Cancel a fresh run, reload the panel, decline then explicitly resume.
  yes();run->click();assert(waitFor([&]{return safe->isEnabled();}));stop->click();assert(waitFor([&]{return resume->isEnabled();}));
  auto selected=panel.findChild<QComboBox*>("vanityRuns")->currentData().toMap();assert(selected.value("checked").toInt()==100);panel.close();
  VanityPanel reopened(cli,static_cast<UiLanguage>(language));reopened.setStyleSheet(themeStyleSheet(language%2?UiTheme::Dark:UiTheme::Light));reopened.show();
  auto* resumeAgain=reopened.findChild<QPushButton*>("vanityResume");assert(waitFor([&]{return resumeAgain->isEnabled();}));
  QTimer::singleShot(0,[]{for(auto* w:QApplication::topLevelWidgets())if(auto* m=qobject_cast<QMessageBox*>(w)){assert(m->text().contains("100")&&m->text().contains("200"));m->button(QMessageBox::No)->click();}});resumeAgain->click();assert(resumeAgain->isEnabled());
  yes();resumeAgain->click();assert(waitFor([&]{return reopened.findChild<QPushButton*>("vanitySafe")->isEnabled();}));reopened.close();assert(reopened.isVisible());
  assert(waitFor([&]{return reopened.findChild<QPushButton*>("vanityRun")->isEnabled();}));assert(!resumeAgain->isEnabled());
  QFile last(tmp.filePath("last-execution-argv"));assert(last.open(QIODevice::ReadOnly));auto values=QJsonDocument::fromJson(last.readAll()).array();assert(values.contains("measured-resume")&&values.contains("--expected-state")&&values.contains(QString(64,'a'))&&values.contains(selected.value("path").toString()));
  reopened.close();panel.show();verify->click();assert(waitFor([&]{return run->isEnabled();}));assert(!resume->isEnabled());
  yes();bench->click();assert(waitFor([&]{return panel.findChild<QPlainTextEdit*>("vanityOutput")->toPlainText().contains("synthetic measurement");}));assert(!safe->isEnabled());stop->click();assert(!stop->isEnabled());assert(waitFor([&]{return run->isEnabled();}));
  write(tmp.filePath("fail"),"fail");verify->click();assert(waitFor([&]{return verify->isEnabled();}));assert(!run->isEnabled()&&!bench->isEnabled());QFile::remove(tmp.filePath("fail"));verify->click();assert(waitFor([&]{return run->isEnabled();}));panel.close();
 }
 std::cout<<"VanityPanelTests passed: four languages, selection, declined measurement, progress, Safe Stop, cancellation, close guard, persisted run selection, confirmed resume, replay blocking and failed verification.\n";
}

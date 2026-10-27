#include "MainWindow.hpp"
#include "VanityPanel.hpp"
#include "GpuControlPanel.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <cassert>
#include <functional>
#include <iostream>
using namespace openpuzzle::ui;
namespace {
template<class T>T* control(MainWindow& window,const char* name){for(auto* p:window.findChildren<T*>())if(p->property("controlId").toString()==name)return p;assert(false);return nullptr;}
bool until(const std::function<bool()>& condition){QElapsedTimer t;t.start();do{QApplication::processEvents();if(condition())return true;QThread::msleep(10);}while(t.elapsed()<7000);return condition();}
void write(const QString& path,const QByteArray& data){QFile f(path);assert(f.open(QIODevice::WriteOnly|QIODevice::Truncate));assert(f.write(data)==data.size());}
QByteArray read(const QString& path){QFile f(path);if(!f.open(QIODevice::ReadOnly))return {};return f.readAll();}
void heldMouseClick(QPushButton* button){
 assert(button->isEnabled());const auto position=QPointF(button->rect().center());const auto global=QPointF(button->mapToGlobal(button->rect().center()));
 QMouseEvent press(QEvent::MouseButtonPress,position,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(button,&press);assert(button->isDown());
 QElapsedTimer t;t.start();while(t.elapsed()<180){QApplication::processEvents();QThread::msleep(5);}
 assert(button->isEnabled()&&button->isDown());
 QMouseEvent release(QEvent::MouseButtonRelease,position,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(button,&release);
}
void answer(QMessageBox::StandardButton choice){QTimer::singleShot(0,[choice]{for(auto* w:QApplication::topLevelWidgets())if(auto* m=qobject_cast<QMessageBox*>(w))m->button(choice)->click();});}
VanityPanel* panel(MainWindow& w){for(auto* p:w.findChildren<QDialog*>())if(auto* v=dynamic_cast<VanityPanel*>(p))return v;assert(false);return nullptr;}
}
int main(int argc,char** argv){
 QApplication app(argc,argv);app.setOrganizationName("OpenPuzzle Tests");app.setApplicationName("UiVanityIntegrationTests");
 QTemporaryDir tmp;assert(tmp.isValid());qputenv("HOME",tmp.path().toUtf8());qputenv("XDG_CONFIG_HOME",tmp.filePath("config").toUtf8());qputenv("XDG_DATA_HOME",tmp.filePath("data").toUtf8());qputenv("OPENPUZZLE_VANITY_STORE",tmp.path().toUtf8());
 const auto cli=tmp.filePath("cli ç ' $()");
 write(cli,R"PY(#!/usr/bin/env python3
import json,os,sys,time,signal
from pathlib import Path
store=Path(os.environ['OPENPUZZLE_VANITY_STORE']);args=sys.argv[1:]
with (store/'all-argv').open('a') as f:f.write(json.dumps(args)+'\n')
if args[0]=='status':
 print((store/'status').read_text());sys.exit()
if args[:2]==['gpu-control','list']:
 print('OpenPuzzle-GPU-controls 1');sys.exit()
if args[0] not in ('vanity-preview','run'):
 (store/'unexpected').write_text(json.dumps(args));sys.exit(3)
action=args[1]
coordinator=args[0]=='run'
if coordinator:
 assert args[:7]==['run','20','--engine','vanity','--backend','cuda','--vanity-session']
 action={'start':'measured-test','resume':'measured-resume','runs':'measured-runs'}[args[7]]
if action=='measured-show':
 if (store/'fail').exists():print('damaged');sys.exit(2)
 print(json.dumps(dict(status='ready',median_mkeys=2597.33,planning_mkeys=2519.41,gpu='Synthetic RTX 4070 SUPER',grid=1024,threads=256,points=1024)));sys.exit()
if action=='measured-runs':
 rows=[json.loads((store/'saved-run').read_text())] if (store/'saved-run').exists() else []
 print(json.dumps(dict(status='local-runs',runs=rows)));sys.exit()
(store/'last-execution-argv').write_text(json.dumps(args))
root=None
if action in ('measured-test','measured-resume'):
 root=Path(args[args.index('--run-root')+1]);control=root/('native/controls' if coordinator else 'controls');control.mkdir(parents=True,exist_ok=True)
 if action=='measured-resume' and (control/'cancel').exists():(control/'cancel').unlink()
def stop(*_):
 if root:(store/'saved-run').write_text(json.dumps(dict(path=str(root),available=True,checked=100,total=200,state_sha256='a'*64)))
 print('cancelled',flush=True);sys.exit(130)
signal.signal(signal.SIGTERM,signal.SIG_IGN if (store/'ignore-term').exists() else stop)
if root:
 print(json.dumps(dict(status='running',checked=100,total=200,speed_mkeys=123)),flush=True)
 until=time.monotonic()+.5
 while not (control/'safe-stop').exists():
  if (control/'cancel').exists():stop()
  if action=='measured-resume' and time.monotonic()>until:break
  print(json.dumps(dict(status='running',checked=100,total=200,speed_mkeys=123)),flush=True)
  time.sleep(.01)
 if (store/'saved-run').exists():(store/'saved-run').unlink()
 print('{"status":"completed"}',flush=True);sys.exit()
if action=='measured-benchmark':
 print('synthetic measurement',flush=True)
 while True:
  print('synthetic measurement',flush=True);time.sleep(.01)
sys.exit(2)
)PY");
 QFile::setPermissions(cli,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);qputenv("OPENPUZZLE_CLI",cli.toUtf8());
 const char* languages[]={"en","pt","fr","es"};
 for(int language=0;language<4;++language){
  write(tmp.filePath("status"),"Status............. idle\nExecution.......... none\n");write(tmp.filePath("profile-sentinel"),"prior independent profile");QFile::remove(tmp.filePath("last-execution-argv"));
  QSettings settings;settings.clear();settings.setValue("ui/language",languages[language]);settings.setValue("ui/theme",language%2?"dark":"light");settings.setValue("run/mode",0);settings.sync();
  {
   MainWindow w;w.show();auto* mode=control<QComboBox>(w,"executionMode");assert(until([&]{return mode->isEnabled();}));assert(mode->count()==6&&mode->findData(100)==5);
   assert(!w.findChild<QPushButton*>("vanityPreview"));mode->setCurrentIndex(5);auto* p=panel(w);assert(!p->isWindow()&&p->isVisible());
   assert(!p->findChild<QPushButton*>("vanityRun")->isVisible());assert(!control<QSpinBox>(w,"puzzle")->isVisible());assert(!control<QCheckBox>(w,"autoStart")->isVisible());
   auto* start=control<QPushButton>(w,"start");auto* stop=control<QPushButton>(w,"stop");auto* safe=control<QPushButton>(w,"safeStop");auto* resume=control<QPushButton>(w,"resumeVanity");auto* bench=control<QPushButton>(w,"benchmark");
   assert(until([&]{return start->isEnabled();}));assert(!resume->isEnabled());assert(start->text()==p->caption(4));assert(!control<QPushButton>(w,"selfTest")->isEnabled());
   if(language==1&&!qEnvironmentVariableIsEmpty("OPV_MAIN_SCREENSHOT")){for(int i=0;i<10;++i){QApplication::processEvents();QThread::msleep(10);}w.grab().save(qEnvironmentVariable("OPV_MAIN_SCREENSHOT"));}
   answer(QMessageBox::No);start->click();assert(!QFile::exists(tmp.filePath("last-execution-argv")));
   write(tmp.filePath("ignore-term"),"ignore");answer(QMessageBox::Yes);start->click();assert(until([&]{return safe->isEnabled();}));assert(!mode->isEnabled()&&!bench->isEnabled()&&!start->isEnabled());
   assert(control<QLabel>(w,"statusSummary")->text().contains("123.00"));
   auto* lang=control<QComboBox>(w,"language");lang->setCurrentIndex((language+1)%4);assert(control<QLabel>(w,"statusSummary")->text().contains("123.00"));lang->setCurrentIndex(language);
   QTimer::singleShot(0,[]{for(auto* t:QApplication::topLevelWidgets())if(auto* m=qobject_cast<QMessageBox*>(t))m->accept();});w.close();assert(w.isVisible());
   heldMouseClick(stop);assert(until([&]{return resume->isEnabled();}));QFile::remove(tmp.filePath("ignore-term"));auto selected=p->findChild<QComboBox*>("vanityRuns")->currentData().toMap();assert(QFile::exists(selected.value("path").toString()+(qEnvironmentVariable("OPENPUZZLE_VANITY_COORDINATOR")=="1"?"/native/controls/cancel":"/controls/cancel")));auto audit=read(tmp.filePath("ui-controls.jsonl"));assert(audit.contains("stop-requested")&&audit.contains("\"cancel_marker\":true"));assert(!safe->isEnabled());w.close();
  }
  {
   MainWindow w;w.show();auto* mode=control<QComboBox>(w,"executionMode");assert(mode->currentData().toInt()==100);
   auto* p=panel(w);auto* start=control<QPushButton>(w,"start");auto* resume=control<QPushButton>(w,"resumeVanity");auto* safe=control<QPushButton>(w,"safeStop");auto* stop=control<QPushButton>(w,"stop");auto* bench=control<QPushButton>(w,"benchmark");
   assert(until([&]{return resume->isEnabled();}));auto selected=p->findChild<QComboBox*>("vanityRuns")->currentData().toMap();auto before=read(tmp.filePath("last-execution-argv"));answer(QMessageBox::No);resume->click();assert(before==read(tmp.filePath("last-execution-argv")));
   answer(QMessageBox::Yes);resume->click();assert(until([&]{return safe->isEnabled();}));assert(until([&]{return start->isEnabled();}));assert(!resume->isEnabled());
   auto a=QJsonDocument::fromJson(read(tmp.filePath("last-execution-argv"))).array();assert(a.contains(qEnvironmentVariable("OPENPUZZLE_VANITY_COORDINATOR")=="1"?"--vanity-session":"measured-resume")&&a.contains("--expected-state")&&a.contains(QString(64,'a'))&&a.contains(selected.value("path").toString()));
   answer(QMessageBox::Yes);start->click();assert(until([&]{return safe->isEnabled();}));heldMouseClick(safe);assert(!safe->isEnabled());assert(until([&]{return start->isEnabled();}));
   answer(QMessageBox::Yes);bench->click();assert(until([&]{return p->findChild<QPlainTextEdit*>("vanityOutput")->toPlainText().contains("synthetic measurement");}));assert(!safe->isEnabled()&&!mode->isEnabled());heldMouseClick(stop);assert(until([&]{return bench->isEnabled();}));assert(!QFile::exists(tmp.filePath("unexpected")));
   write(tmp.filePath("fail"),"fail");p->findChild<QPushButton*>("vanityVerify")->click();assert(until([&]{return !p->busy();}));assert(!start->isEnabled()&&!resume->isEnabled()&&!bench->isEnabled());QFile::remove(tmp.filePath("fail"));p->findChild<QPushButton*>("vanityVerify")->click();assert(until([&]{return start->isEnabled();}));
   write(tmp.filePath("status"),"Status............. running\nRuntime PID........ 12345\n");control<QPushButton>(w,"refresh")->click();assert(until([&]{return !mode->isEnabled();}));assert(!start->isEnabled()&&!bench->isEnabled()&&!resume->isEnabled());
   write(tmp.filePath("status"),"Status............. idle\nExecution.......... none\n");control<QPushButton>(w,"refresh")->click();assert(until([&]{return mode->isEnabled();}));mode->setCurrentIndex(0);assert(!p->isVisible()&&!resume->isVisible());assert(start->isEnabled()&&start->text()!=p->caption(4));assert(control<QPushButton>(w,"selfTest")->isEnabled());
   assert(read(tmp.filePath("profile-sentinel"))=="prior independent profile");w.close();
  }
 }
 std::cout<<"UiVanityIntegrationTests passed: held mouse Stop/Safe Stop/benchmark cancellation under streamed updates, ignored SIGTERM cancelled by scoped marker, timestamped audit, main selector, shared controls, four languages, progress, reopen/resume, cancellation, Safe Stop, benchmark routing, close guard, invalid profile and occupied-runtime blocking.\n";
}

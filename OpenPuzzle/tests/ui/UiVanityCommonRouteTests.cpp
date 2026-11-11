#include "MainWindow.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDialog>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QFrame>
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
template<class T>T* control(MainWindow& w,const char* id){for(auto* p:w.findChildren<T*>())if(p->property("controlId").toString()==id)return p;assert(false);return nullptr;}
bool until(const std::function<bool()>& f){QElapsedTimer t;t.start();do{QApplication::processEvents();if(f())return true;QThread::msleep(10);}while(t.elapsed()<8000);return f();}
void write(const QString& p,const QByteArray& d){QFile f(p);assert(f.open(QIODevice::WriteOnly|QIODevice::Truncate));assert(f.write(d)==d.size());}
QByteArray read(const QString& p){QFile f(p);if(!f.open(QIODevice::ReadOnly))return {};return f.readAll();}
}
int main(int argc,char** argv){
 QApplication app(argc,argv);app.setOrganizationName("OpenPuzzle Tests");app.setApplicationName("UiVanityCommonRouteTests");
 QTemporaryDir tmp;assert(tmp.isValid());qputenv("HOME",tmp.path().toUtf8());qputenv("XDG_CONFIG_HOME",tmp.filePath("config").toUtf8());qputenv("XDG_DATA_HOME",tmp.filePath("data").toUtf8());
 qputenv("OPENPUZZLE_VANITY_COMMON_PROFILE",tmp.filePath("native-profile").toUtf8());qunsetenv("OPENPUZZLE_VANITY_STORE");
 auto cli=tmp.filePath("cli ç ' $()");qputenv("OPENPUZZLE_CLI",cli.toUtf8());qputenv("OPV_UI_FIXTURE",tmp.path().toUtf8());
 write(cli,R"PY(#!/usr/bin/env python3
import json,os,sys
from pathlib import Path
root=Path(os.environ['OPV_UI_FIXTURE']);args=sys.argv[1:]
with (root/'argv').open('a') as f:f.write(json.dumps(args)+'\n')
if args[0]=='status':
 if (root/'status-fail').exists():sys.exit(7)
 print((root/'status').read_text());sys.exit()
if args[:2]==['gpu-control','list']:print('OpenPuzzle-GPU-controls 1');sys.exit()
if args[0]=='benchmark':print('Vanity automatic benchmark complete');sys.exit()
if args[0]=='run':
 if '--recover-assignment' in args:
  assert args==['run','71','--engine','vanity','--backend','cuda','--device','5',
                '--recover-assignment','28b14e3e-bfad-4be1-a5b6-c1ce8bb0a6fe','--recover-expired'],args
  (root/'recovered-slot').write_text(os.environ.get('OPENPUZZLE_EXECUTION_SLOT',''))
  (root/'status').write_text((root/'running-recovery').read_text())
 else:
  assert args==['run','69','--engine','vanity','--backend','cuda','--device','0'],args
  (root/'status').write_text('Status............. running\nEngine............. Vanity\nBackend............ CUDA\nPuzzle............. 69\nAssignment......... fixture-assignment\n')
 sys.exit()
if args[0]=='stop':
 (root/'status').write_text('Status............. stopped\nEngine............. Vanity\nBackend............ CUDA\nPuzzle............. 69\nAssignment......... fixture-assignment\n');sys.exit()
if args[0]=='safestop':sys.exit()
(root/'unexpected').write_text(json.dumps(args));sys.exit(3)
)PY");assert(QFile::setPermissions(cli,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
 for(int language=0;language<4;++language){
  QSettings settings;settings.clear();settings.setValue("ui/language",QStringList{"en","pt","fr","es"}[language]);settings.sync();
  write(tmp.filePath("status"),"Status............. idle\nExecution.......... none\n");
  MainWindow w;w.show();auto* mode=control<QComboBox>(w,"executionMode");auto* start=control<QPushButton>(w,"start");
  auto* version=w.findChild<QLabel*>("VersionLabel");assert(version&&version->isVisible()&&version->text()==QStringLiteral("OpenPuzzle %1").arg(QStringLiteral(OPENPUZZLE_VERSION)));
  assert(until([&]{return mode->isEnabled();}));int index=mode->findData(static_cast<int>(RunMode::VanityCuda));assert(index>=0);assert(mode->findData(100)<0);
  mode->setCurrentIndex(index);control<QSpinBox>(w,"puzzle")->setValue(69);
  auto* benchmark=control<QPushButton>(w,"benchmark");
  assert(benchmark->isEnabled());assert(control<QPushButton>(w,"resumeVanity")->isHidden());
  if(language==0){
   QTimer::singleShot(0,[&w]{auto* dialog=w.findChild<QDialog*>("linuxBenchmarkDialog");assert(dialog);
    auto* backend=dialog->findChild<QComboBox*>("benchmarkBackend");assert(backend&&!backend->isEnabled()&&backend->currentData()=="cuda");
    dialog->findChild<QSpinBox*>("benchmarkDevice")->setValue(3);
    dialog->findChild<QPushButton*>("benchmarkConfirm")->click();});
   benchmark->click();
   assert(until([&]{return read(tmp.filePath("argv")).contains("[\"benchmark\", \"--backend\", \"cuda\", \"--gpu\", \"3\", \"--auto\", \"--real\", \"--require-idle\", \"--engine\", \"vanity\"]");}));
   assert(until([&]{return benchmark->isEnabled();}));
  }
  control<QSpinBox>(w,"cudaDevice")->setValue(0);
  start->click();assert(until([&]{return control<QPushButton>(w,"stop")->isEnabled();}));
  control<QPushButton>(w,"safeStop")->click();assert(until([&]{return read(tmp.filePath("argv")).contains("[\"safestop\"]");}));
  assert(until([&]{return control<QPushButton>(w,"stop")->isEnabled();}));
  QTimer::singleShot(0,[]{for(auto* p:QApplication::topLevelWidgets())if(auto* box=qobject_cast<QMessageBox*>(p))box->button(QMessageBox::Yes)->click();});
  control<QPushButton>(w,"stop")->click();assert(until([&]{return start->isEnabled();}));
  assert(start->text().contains(language==0?"Resume":language==1?"Retomar":language==2?"Reprendre":"Reanudar"));
  assert(!read(tmp.filePath("argv")).contains("vanity-preview"));assert(!QFile::exists(tmp.filePath("unexpected")));w.close();
 }
 // A reopened ordinary UI finds the shared paused state and selects its puzzle.
 {MainWindow w;w.show();auto* start=control<QPushButton>(w,"start");assert(until([&]{return start->isEnabled();}));assert(control<QComboBox>(w,"executionMode")->currentData().toInt()==static_cast<int>(RunMode::VanityCuda));assert(control<QSpinBox>(w,"puzzle")->value()==69);w.close();}
 {
  write(tmp.filePath("status"),"Status............. idle\nExecution.......... none\n");
  QSettings settings;
  settings.setValue("ui/language","en");
  settings.sync();
  MainWindow w;
  w.show();
  auto* refresh=control<QPushButton>(w,"refresh");
  auto* details=control<QPlainTextEdit>(w,"statusOutput");
  assert(until([&]{return control<QPushButton>(w,"start")->isEnabled();}));
  for(auto* timer:w.findChildren<QTimer*>()) {
   if(!timer->isSingleShot() && timer->interval()==3000) timer->stop();
  }
  const QByteArray stopped=
      "Slot............... cuda-5\nStatus............. stopped\n"
      "Engine............. Vanity\nBackend............ CUDA\nDevice............. 5\n"
      "Puzzle............. 71\nAssignment number... 10677\n"
      "Assignment......... 28b14e3e-bfad-4be1-a5b6-c1ce8bb0a6fe\n"
      "Exit code.......... 130\nStop reason........ interrupted\n"
      "Stopped at......... 2026-11-11 13:00:00 UTC\n"
      "Checkpoint......... verified locally (4096 keys checked)\n";
  const QByteArray peer=
      "\nSlot............... cuda-0\nStatus............. running\n"
      "Engine............. Vanity\nBackend............ CUDA\nPuzzle............. 71\n";
  const auto query=[&](const QByteArray& state) {
   write(tmp.filePath("status"),state);
   assert(until([&]{return refresh->isEnabled();}));
   refresh->click();
   assert(until([&]{return details->toPlainText()==QString::fromUtf8(state).trimmed();}));
   QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
  };
  query(stopped+peer);
  auto* recover=control<QPushButton>(w,"recoverVanitySlot");
  assert(recover->isEnabled());
  assert(recover->property("executionSlot").toString()=="cuda-5");
  assert(!control<QPushButton>(w,"start")->isEnabled());
  assert(control<QPushButton>(w,"start")->text()=="Start");
  QComboBox* language=nullptr;
  for(auto* combo:w.findChildren<QComboBox*>()) {
   if(combo->findData("pt")>=0 && combo->findData("fr")>=0) language=combo;
  }
  assert(language);
  const QStringList checkpoints={"Verified locally","Validado localmente","Vérifié localement","Validado localmente"};
  const QStringList recoverLabels={"Recover","Recuperar","Récupérer","Recuperar"};
  for(int i=0;i<4;++i) {
   language->setCurrentIndex(i);
   QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
   recover=control<QPushButton>(w,"recoverVanitySlot");
   assert(recover->text()==recoverLabels[i] && recover->isEnabled());
   assert(until([&] {
    bool visibleProof=false,visibleTime=false;
    for(auto* label:w.findChildren<QLabel*>()) {
     if(!label->isVisible()) continue;
     visibleProof=visibleProof || label->text().contains(checkpoints[i]);
     visibleTime=visibleTime || label->text().contains("2026-11-11 13:00:00 UTC");
    }
    return visibleProof && visibleTime;
   }));
  }
  language->setCurrentIndex(0);
  auto invalid=stopped;
  invalid.replace("verified locally (4096 keys checked)","review required");
  query(invalid+peer);
  assert(!control<QPushButton>(w,"recoverVanitySlot")->isEnabled());
  invalid=stopped;
  invalid.replace("Device............. 5\n","");
  query(invalid+peer);
  assert(!control<QPushButton>(w,"recoverVanitySlot")->isEnabled());
  invalid=peer;
  invalid.replace("Status............. running","Status............. identity unavailable");
  query(stopped+invalid);
  assert(!control<QPushButton>(w,"recoverVanitySlot")->isEnabled());
  query(stopped+peer);
  write(tmp.filePath("status-fail"),"fail");
  refresh->click();
  assert(until([&]{return control<QLabel>(w,"statusBadge")->text()=="Status unavailable";}));
  assert(!control<QPushButton>(w,"recoverVanitySlot")->isEnabled());
  assert(QFile::remove(tmp.filePath("status-fail")));
  refresh->click();
  assert(until([&]{return control<QPushButton>(w,"recoverVanitySlot")->isEnabled();}));
  const auto before=read(tmp.filePath("argv")).count("recover-assignment");
  QTimer::singleShot(0,[]{for(auto* widget:QApplication::topLevelWidgets()) {
   if(auto* box=qobject_cast<QMessageBox*>(widget))box->button(QMessageBox::No)->click();
  }});
  control<QPushButton>(w,"recoverVanitySlot")->click();
  assert(read(tmp.filePath("argv")).count("recover-assignment")==before);
  auto running=stopped;
  running.replace("Status............. stopped","Status............. running");
  write(tmp.filePath("running-recovery"),running+peer);
  QTimer::singleShot(0,[]{for(auto* widget:QApplication::topLevelWidgets()) {
   if(auto* box=qobject_cast<QMessageBox*>(widget))box->button(QMessageBox::Yes)->click();
  }});
  control<QPushButton>(w,"recoverVanitySlot")->click();
  assert(until([&]{return read(tmp.filePath("recovered-slot"))=="cuda-5";}));
  assert(read(tmp.filePath("argv")).count("recover-assignment")==before+1);
  assert(until([&]{return details->toPlainText()==QString::fromUtf8(running+peer).trimmed();}));
  QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
  assert(!w.findChildren<QPushButton*>().isEmpty());
  for(auto* button:w.findChildren<QPushButton*>()) {
   assert(button->property("controlId").toString()!="recoverVanitySlot");
  }
  assert(!QFile::exists(tmp.filePath("unexpected")));
  w.close();
 }
 std::cout<<"Shared main controls: ordinary Vanity argv, automatic benchmark, stop/safestop, paused reopen/resume, four languages and no legacy route.\n";
}

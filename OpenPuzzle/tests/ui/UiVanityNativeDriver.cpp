#include "MainWindow.hpp"
#include <QApplication>
#include <QComboBox>
#include <QElapsedTimer>
#include <QFile>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QThread>
#include <QTimer>
#include <cassert>
#include <functional>
#include <iostream>
using namespace openpuzzle::ui;
namespace {
template<class T>T* control(MainWindow& w,const char* id){for(auto* p:w.findChildren<T*>())if(p->property("controlId").toString()==id)return p;assert(false);return nullptr;}
bool until(const std::function<bool()>& f){QElapsedTimer t;t.start();do{QApplication::processEvents();if(f())return true;QThread::msleep(10);}while(t.elapsed()<20000);return f();}
qulonglong checked(const QString& path){QFile f(path+"/vanity-native/checkpoint.txt");if(!f.open(QIODevice::ReadOnly))return 0;auto parts=f.readAll().simplified().split(' ');return parts.size()==9?parts[6].toULongLong():0;}
}
int main(int argc,char** argv){
 QApplication app(argc,argv);app.setApplicationName("OpenPuzzle");app.setOrganizationName("OpenPuzzle Project");
 assert(argc==2);QString phase=argv[1];assert(phase=="pause"||phase=="resume-safe");
 QString workspace=qEnvironmentVariable("OPV_ACCEPTANCE_WORKSPACE"),data=qEnvironmentVariable("XDG_DATA_HOME")+"/OpenPuzzle";
 assert(!workspace.isEmpty());QSettings settings;settings.setValue("ui/language","en");settings.setValue("run/mode",static_cast<int>(RunMode::VanityCuda));settings.setValue("run/puzzle",69);settings.sync();
 MainWindow w;w.show();auto* start=control<QPushButton>(w,"start");auto* mode=control<QComboBox>(w,"executionMode");
 assert(until([&]{return start->isEnabled();}));mode->setCurrentIndex(mode->findData(static_cast<int>(RunMode::VanityCuda)));control<QSpinBox>(w,"puzzle")->setValue(69);control<QSpinBox>(w,"cudaDevice")->setValue(0);
 if(phase=="resume-safe")assert(start->text().contains("Resume"));
 auto prior=checked(workspace);start->click();
 assert(until([&]{return control<QPushButton>(w,"stop")->isEnabled()&&checked(workspace)>prior;}));
 if(phase=="pause"){
  QTimer::singleShot(0,[]{for(auto* p:QApplication::topLevelWidgets())if(auto* box=qobject_cast<QMessageBox*>(p))box->button(QMessageBox::Yes)->click();});
  control<QPushButton>(w,"stop")->click();assert(until([&]{return start->isEnabled()&&start->text().contains("Resume")&&!QFile::exists(data+"/runtime.pid");}));
  assert(QFile::exists(data+"/client.state"));
 }else{
  control<QPushButton>(w,"safeStop")->click();
  assert(until([&]{return !QFile::exists(data+"/client.state")&&!QFile::exists(data+"/runtime.pid")&&start->isEnabled();}));
 }
 assert(!control<QPushButton>(w,"benchmark")->isEnabled());w.close();
 std::cout<<"Actual MainWindow + ordinary native CLI: "<<phase.toStdString()<<" validated.\n";
}

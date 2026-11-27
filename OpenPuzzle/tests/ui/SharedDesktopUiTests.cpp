#include "MainWindow.hpp"
#include "GpuControlPanel.hpp"
#include <QLabel>
#include "openpuzzle/ui/RunCommandBuilder.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <iostream>
#include <functional>
using namespace openpuzzle::ui;
namespace {
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
bool until(const std::function<bool()>& condition){QElapsedTimer timer;timer.start();do{QApplication::processEvents();if(condition())return true;QThread::msleep(10);}while(timer.elapsed()<9000);return condition();}
void write(const QString& path,const QByteArray& bytes){QFile f(path);check(f.open(QIODevice::WriteOnly|QIODevice::Truncate),"Fixture write open");check(f.write(bytes)==bytes.size(),"Fixture write");}
template<class T>T* control(MainWindow& w,const char* name){for(auto* p:w.findChildren<T*>())if(p->property("controlId").toString()==name)return p;throw std::runtime_error(std::string("Missing control: ")+name);}
int fixture(const QStringList& a){const auto root=qEnvironmentVariable("OPENPUZZLE_QT_FIXTURE_ROOT");const auto command=a.value(1);
#ifdef OPENPUZZLE_DESKTOP_WINDOWS
 if(QFileInfo(a.value(0)).fileName().compare("openpuzzle-vanity-cuda-test.exe",Qt::CaseInsensitive)==0){
  write(root+"/vanity-device",command.toUtf8());
  if(QFile::exists(root+"/fail-vanity")){std::cout<<"Synthetic CUDA failure\n";return 1;}
  std::cout<<"PASS: Vanity CUDA verified 513 keys and found the synthetic target.\n";return 0;
 }
#endif
 if(command=="desktop-capabilities"){
  if(QFile::exists(root+"/amd-only")){std::cout<<"{\"cuda\":[],\"opencl\":[{\"engine_index\":0,\"platform_index\":0,\"native_index\":0,\"gpu\":true,\"name\":\"gfx1010:xnack-\",\"memory_bytes\":8573157376,\"display_name\":\"AMD Radeon RX 5700 XT\"}]}";return 0;}
  if(QFile::exists(root+"/fail-discovery")){std::cout<<"{\"cuda\":[],\"opencl\":[],\"diagnostics\":{\"opencl\":{\"code\":1,\"message\":\"AMD enumeration failed: synthetic driver error\"}},\"hardware_report\":\"Windows adapter: AMD Radeon RX 5700 XT\"}";return 0;}
  std::cout<<"{\"cuda\":[{\"engine_index\":0,\"name\":\"synthetic CUDA GPU\",\"display_name\":\"NVIDIA RTX 3080 (fixture)\"}],\"opencl\":[{\"engine_index\":7,\"name\":\"gfx1010\",\"display_name\":\"AMD RX 5700 (fixture)\"}]}\n";return 0;}
 if(command=="devices"){
  std::cout << "CUDA: DETECTED\nInventory index: 0\nDevice: NVIDIA GeForce RTX 3080\nDriver: CUDA API 12.8\n"
               "OpenCL: DETECTED\nInventory index: 7\nDevice: gfx1010:xnack-\nDriver: 3652.0 (PAL,LC)\n"
               "Physical device 7: UUID GPU-private-serial; PCI 0000:09:00.0\n"
               "Display name: AMD Radeon RX 5700 XT\n"
               "Windows display adapters: 3\nWindows adapter: AMD Radeon RX 5700 XT\n"
               "Windows adapter: Intel Arc A380\nWindows device: PCI\\PRIVATE-ID\n"
               "Windows adapter: NVIDIA RTX token_hidden\n";
  return 2;
 }
 if(command=="status"){
  if(QFile::exists(root+"/fail-status"))return 1;
  if(QFile::exists(root+"/active"))std::cout<<"Slot............... primary\nStatus............. running\nAssignment......... synthetic-running\nEngine............. KeyHunt\nBackend............ CPU\nSpeed.............. 2 MKey/s\nKeys checked....... 20\n";
  else std::cout<<"Slot............... primary\nStatus............. idle\n";
  if(QFile::exists(root+"/preserved"))std::cout<<"Slot............... opencl-7\nStatus............. stopped\nAssignment......... synthetic-preserved\nPuzzle............. 71\nAssignment number... 857\nRecovery available.......... yes\nSynchronization.... pending supervisor\nTemperature.......... 64.0 °C (GPU)\nPower.......... 142.8 W / —\n";
  std::cout<<"Desktop profile.... "<<(QFile::exists(root+"/measured")?"ready":"missing")<<'\n';return 0;
 }
 if(command=="benchmark"){write(root+"/benchmark-argv",a.mid(1).join('\n').toUtf8());write(root+"/measured","yes");std::cout<<"Synthetic measured profile saved\n";return 0;}
 if(command=="run"){write(root+"/run-argv",a.mid(1).join('\n').toUtf8());write(root+"/active","yes");return 0;}
 if(command=="safestop"){write(root+"/safestop-argv",a.mid(1).join('\n').toUtf8());QFile::remove(root+"/active");return 0;}
 if(command=="gpu-control"){std::cout<<"OpenPuzzle-GPU-controls 1\n";return 0;}
 if(command=="doctor"||command=="selftest"){std::cout<<"Synthetic diagnostics passed\n";return 0;}
 if(command=="recover"){write(root+"/recover-argv",a.mid(1).join('\n').toUtf8());QFile::remove(root+"/preserved");return 0;}
 write(root+"/unexpected-command",a.join('\n').toUtf8());return 1;
}
}
int main(int argc,char** argv){try{
 QStringList args;for(int i=0;i<argc;++i)args<<QString::fromLocal8Bit(argv[i]);
 if(argc>1)return fixture(args);
 QApplication app(argc,argv);app.setOrganizationName("OpenPuzzle Qt Tests");app.setApplicationName("SharedDesktopUiTests");
 QTemporaryDir temp;check(temp.isValid(),"Temporary directory");
 qputenv("HOME",temp.path().toUtf8());qputenv("LOCALAPPDATA",temp.path().toUtf8());qputenv("XDG_DATA_HOME",temp.filePath("data").toUtf8());qputenv("XDG_CONFIG_HOME",temp.filePath("config").toUtf8());
 qputenv("OPENPUZZLE_QT_FIXTURE_ROOT",temp.path().toUtf8());qputenv("OPENPUZZLE_CLI",QCoreApplication::applicationFilePath().toUtf8());
 QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.filePath("settings"));QSettings().clear();
 MainWindow window;window.resize(1180,940);window.show();
 auto* start=control<QPushButton>(window,"start");auto* refresh=control<QPushButton>(window,"refresh");auto* benchmark=control<QPushButton>(window,"benchmark");
 auto* mode=control<QComboBox>(window,"executionMode");auto* device=control<QSpinBox>(window,"cudaDevice");auto* threads=control<QSpinBox>(window,"cpuThreads");
#ifdef OPENPUZZLE_DESKTOP_WINDOWS
 check(mode->findData(static_cast<int>(RunMode::VanityCuda))<0,"Vanity test offered without bundled binaries");
#endif
 check(until([&]{return benchmark->isEnabled()&&device->toolTip().contains("3080");}),"Initial desktop discovery/status");
 check(!start->isEnabled()&&control<QSpinBox>(window,"puzzle")->value()==0,"Unmeasured desktop started or nonautomatic puzzle");
 check(!control<QCheckBox>(window,"autoStart")->isEnabled()&&!control<QPushButton>(window,"installKangaroo")->isEnabled(),"Unsupported Windows action enabled");
 mode->setCurrentIndex(mode->findData(static_cast<int>(RunMode::BitCrackOpencl)));device->setValue(7);
 check(until([&]{return device->toolTip().contains("5700")&&benchmark->isEnabled();}),"AMD engine index/name lost");
 auto measure=[&](const QString& backend,int cpuThreads){
  check(until([&]{return benchmark->isEnabled();}),"Benchmark unavailable");
  QTimer::singleShot(0,[&]{auto* d=window.findChild<QDialog*>("linuxBenchmarkDialog");check(d,"Shared benchmark dialog absent");auto* b=d->findChild<QComboBox*>("benchmarkBackend");check(b,"Benchmark backend absent");b->setCurrentIndex(b->findData(backend));if(backend=="cpu")d->findChild<QSpinBox*>("benchmarkThreads")->setValue(cpuThreads);else d->findChild<QSpinBox*>("benchmarkDevice")->setValue(7);d->findChild<QPushButton*>("benchmarkConfirm")->click();});
  benchmark->click();check(until([&]{return start->isEnabled();}),"Measured profile did not enable Start");
 };
 measure("opencl",1);check(mode->currentData().toInt()==static_cast<int>(RunMode::BitCrackOpencl)&&device->value()==7,"GPU benchmark selection lost");
 mode->setCurrentIndex(mode->findData(static_cast<int>(RunMode::KeyHuntCpu)));measure("cpu",4);check(threads->value()==4,"CPU benchmark thread selection lost");
 // Exercise the actual detached Windows launcher and cooperative command route.
 start->click();auto* safeStop=control<QPushButton>(window,"safeStop");
 check(until([&]{return QFile::exists(temp.filePath("run-argv"))&&safeStop->isEnabled();}),"Detached desktop run did not become active");
 safeStop->click();check(until([&]{return QFile::exists(temp.filePath("safestop-argv"))&&start->isEnabled();}),"Safe Stop did not settle");
 // A preserved assignment blocks a new claim and offers the native recovery route.
 write(temp.filePath("preserved"),"yes");refresh->click();check(until([&]{return !start->isEnabled()&&!window.findChildren<QPushButton*>().isEmpty()&&window.findChild<QWidget*>("StatusSlots")->findChildren<QPushButton*>().size()>0;}),"Preserved assignment not shown");
 check(until([&]{bool temperatureVisible=false,powerVisible=false;
  for(auto* label:window.findChildren<QLabel*>()){const bool laidOut=label->isVisible()&&label->height()>10;
   temperatureVisible=temperatureVisible||(laidOut&&label->text()=="64.0 °C (GPU)");powerVisible=powerVisible||(laidOut&&label->text()=="142.8 W / —");}
  return temperatureVisible&&powerVisible;
 }),"Shared status cards omitted visible Windows GPU metrics");
 const auto metricsArtifacts=qEnvironmentVariable("OPENPUZZLE_QT_TEST_ARTIFACTS");
 if(!metricsArtifacts.isEmpty()){QDir().mkpath(metricsArtifacts);check(window.grab().save(metricsArtifacts+"/metrics.png"),"GPU metrics screenshot failed");}
 auto* diagnostic = control<QPushButton>(window, "copyDiagnostic");
 auto* messagesOutput = control<QPlainTextEdit>(window, "messagesOutput");
 messagesOutput->setPlainText(
     "Windows run refused: Vanity lease refused: {\"error\":\"assignment_not_found\",\"message\":\"private@example.test\"}\n"
     "Error: authorization Bearer token_hidden; C:\\Users\\Private; https://private.example.test/a\n"
     "Private key: 0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
 QApplication::clipboard()->clear();
 diagnostic->click();
 check(until([&]{return diagnostic->isEnabled() && QApplication::clipboard()->text().contains("Range #857");}),
       "Windows support report was not copied");
 const QString report = QApplication::clipboard()->text();
 for (const QString& expected : QStringList{"Platform: Windows", "Selected mode: KeyHunt", "CPU threads: 4",
                                 "AMD Radeon RX 5700 XT", "3652.0 (PAL,LC)", "CUDA API 12.8",
                                 "Intel Arc A380", "stopped", "pending supervisor", "assignment_not_found"}) {
   if (!report.contains(expected))
     throw std::runtime_error("Windows support report omitted: " + expected.toStdString());
 }
 for (const QString& secret : QStringList{"private@example.test", "token_hidden", "Private", "https://", "synthetic-preserved",
                               "0123456789abcdef", "GPU-private-serial", "0000:09:00.0", "PCI\\PRIVATE-ID"}) {
   check(!report.contains(secret), "Windows support report leaked private data");
 }
 auto* recover=control<QPushButton>(window,"recoverWindowsSlot");check(recover->isEnabled(),"Recovery unavailable");recover->click();check(until([&]{return QFile::exists(temp.filePath("recover-argv"));}),"Recovery not dispatched");
 check(until([&]{return start->isEnabled();}),"Recovered status did not settle");
 write(temp.filePath("fail-status"),"yes");refresh->click();check(until([&]{return !start->isEnabled()&&!benchmark->isEnabled();}),"Failed status allowed a new claim");QFile::remove(temp.filePath("fail-status"));refresh->click();check(until([&]{return start->isEnabled();}),"Status retry failed");
 // A known Windows adapter with a failed engine probe must show the
 // diagnostic, keep old measured data, and allow retry after the fault clears.
 mode->setCurrentIndex(mode->findData(static_cast<int>(RunMode::BitCrackOpencl)));device->setValue(7);
 check(until([&]{return start->isEnabled();}),"AMD selection did not settle");
 write(temp.filePath("fail-discovery"),"yes");refresh->click();
 auto* output=control<QPlainTextEdit>(window,"messagesOutput");
 check(until([&]{return output->toPlainText().contains("synthetic driver error")&&!start->isEnabled()&&!benchmark->isEnabled();}),"Failed GPU discovery was hidden or allowed a new search");
 check(output->toPlainText().contains("AMD Radeon RX 5700 XT"),"Windows adapter evidence was lost");
 QFile::remove(temp.filePath("fail-discovery"));refresh->click();
 check(until([&]{return start->isEnabled()&&benchmark->isEnabled()&&device->toolTip().contains("5700");}),"GPU detection did not recover on Refresh");
 auto* language=control<QComboBox>(window,"language");for(int i=0;i<language->count();++i){language->setCurrentIndex(i);check(!start->text().isEmpty(),"Translation lost");}
 language->setCurrentIndex(0);
 const auto destination=qEnvironmentVariable("OPENPUZZLE_QT_TEST_ARTIFACTS");if(!destination.isEmpty()){
  QDir().mkpath(destination);check(window.grab().save(destination+"/panel.png"),"Screenshot failed");
  QJsonArray controls;for(auto* w:window.findChildren<QWidget*>()){const auto id=w->property("controlId").toString();if(!id.isEmpty())controls.append(QJsonObject{{"id",id},{"class",w->metaObject()->className()}});}write(destination+"/controls.json",QJsonDocument(controls).toJson());
 }
 check(!QFile::exists(temp.filePath("unexpected-command")),"Unexpected command / research claim");
 for(auto* p:window.findChildren<QProcess*>())if(p->state()!=QProcess::NotRunning){p->kill();p->waitForFinished(3000);}
 window.close();for(auto* timer:window.findChildren<QTimer*>())timer->stop();
#ifdef OPENPUZZLE_DESKTOP_WINDOWS
 {
  const QDir folder(QCoreApplication::applicationDirPath());
  const auto launcher=folder.filePath("openpuzzle-vanity-cuda-test.exe");
  const auto engine=folder.filePath("engines/vanity-offline.exe");
  check(!QFile::exists(launcher)&&!QFile::exists(engine),"Vanity fixture needs an isolated test directory");
  const struct RemoveFixtures {
   QString launcher,engine;
   ~RemoveFixtures(){QFile::remove(launcher);QFile::remove(engine);}
  } removeFixtures{launcher,engine};
  check(QDir().mkpath(QFileInfo(engine).absolutePath()),"Vanity fixture engine directory");
  write(engine,"synthetic");
  check(QFile::copy(QCoreApplication::applicationFilePath(),launcher),"Vanity launcher fixture");
  QSettings().clear();
  QFile::remove(temp.filePath("measured"));
  QFile::remove(temp.filePath("run-argv"));
  MainWindow vanity;vanity.show();
  auto* vanityMode=control<QComboBox>(vanity,"executionMode");
  auto* runTest=control<QPushButton>(vanity,"start");
  auto* hint=control<QLabel>(vanity,"windowsVanityTestHint");
  auto* messages=control<QPlainTextEdit>(vanity,"messagesOutput");
  const int testMode=vanityMode->findData(static_cast<int>(RunMode::VanityCudaLocalTest));
  check(testMode>=0&&vanityMode->model()->flags(vanityMode->model()->index(testMode,0)).testFlag(Qt::ItemIsEnabled),
        "Bundled Vanity mode is unavailable");
  vanityMode->setCurrentIndex(testMode);
  check(until([&]{return runTest->isEnabled();}),"Local Vanity test requires a production profile");
  check(hint->isVisible()&&!control<QSpinBox>(vanity,"puzzle")->isVisible()&&
        !control<QPushButton>(vanity,"benchmark")->isEnabled(),"Local Vanity test shows production controls");
  runTest->click();
  check(until([&]{return runTest->isEnabled()&&hint->text().contains("passed");}),"Vanity success not shown");
  QFile selectedDevice(temp.filePath("vanity-device"));
  check(selectedDevice.open(QIODevice::ReadOnly)&&selectedDevice.readAll()=="0","Wrong CUDA device sent to Vanity");
  check(!QFile::exists(temp.filePath("run-argv")),"Local Vanity test requested production work");
  check(messages->toPlainText().contains("PASS"),"Vanity output missing");
  write(temp.filePath("fail-vanity"),"yes");runTest->click();
  check(until([&]{return hint->text().contains("failed")&&messages->toPlainText().contains("Synthetic CUDA failure");}),
        "Vanity failure not shown");
  vanity.close();
  check(QFile::remove(launcher),"Could not remove the optional Vanity test launcher");
  QSettings().clear();
  MainWindow release;release.show();
  auto* releaseMode=control<QComboBox>(release,"executionMode");
  auto* startVanity=control<QPushButton>(release,"start");
  const int coordinatedMode=releaseMode->findData(static_cast<int>(RunMode::VanityCuda));
  check(coordinatedMode>=0,"Coordinated Vanity mode is unavailable");
  check(releaseMode->findData(static_cast<int>(RunMode::VanityCudaLocalTest))<0,
        "The optional Vanity test mode appeared without its launcher");
  releaseMode->setCurrentIndex(coordinatedMode);
  auto* cudaBenchmark=control<QPushButton>(release,"benchmark");
  auto* vanityPuzzle=control<QSpinBox>(release,"puzzle");
  check(until([&]{return cudaBenchmark->isEnabled()&&vanityPuzzle->isVisible()&&vanityPuzzle->value()==71;})&&
        !startVanity->isEnabled(),"Coordinated Vanity ignored the missing CUDA profile or default puzzle");
  QTimer::singleShot(0,[&]{
    auto* dialog=release.findChild<QDialog*>("linuxBenchmarkDialog");
    check(dialog&&dialog->findChild<QComboBox*>("benchmarkBackend")->currentData()=="cuda","Vanity benchmark is not using CUDA");
    dialog->accept();
  });
  cudaBenchmark->click();
  check(until([&]{return startVanity->isEnabled()&&releaseMode->currentIndex()==coordinatedMode;}),
        "CUDA benchmark did not retain the selected Vanity mode");
  startVanity->click();
  check(until([&]{return QFile::exists(temp.filePath("run-argv"))&&control<QPushButton>(release,"safeStop")->isEnabled();}),
        "Coordinated Vanity did not dispatch a run");
  QFile coordinatedArgs(temp.filePath("run-argv"));
  check(coordinatedArgs.open(QIODevice::ReadOnly)&&coordinatedArgs.readAll()=="run\n71\n--engine\nvanity\n--backend\ncuda\n--device\n0\n--duration-minutes\n5",
        "Coordinated Vanity used the wrong client arguments");
  control<QPushButton>(release,"safeStop")->click();
  check(until([&]{return !QFile::exists(temp.filePath("active"));}),"Coordinated Vanity Safe Stop command was not dispatched");
 }
#endif
 write(temp.filePath("amd-only"),"yes");
 for(const int saved:{-1,static_cast<int>(RunMode::BitCrackCuda),static_cast<int>(RunMode::BitCrackOpencl),static_cast<int>(RunMode::KeyHuntCpu)}){
  QSettings settings;settings.clear();
  if(saved>=0){settings.setValue("run/mode",saved);settings.setValue("run/device",7);}settings.sync();
  MainWindow rx;rx.show();auto* rxMode=control<QComboBox>(rx,"executionMode");auto* rxDevice=control<QSpinBox>(rx,"cudaDevice");
  const auto expected=saved==static_cast<int>(RunMode::KeyHuntCpu)?RunMode::KeyHuntCpu:RunMode::BitCrackOpencl;
  const bool settled=until([&]{return rxMode->currentData().toInt()==static_cast<int>(expected)&&control<QPushButton>(rx,"benchmark")->isEnabled()&&
   (expected==RunMode::KeyHuntCpu||(rxDevice->value()==0&&rxDevice->toolTip().contains("AMD Radeon RX 5700 XT")));});
  if(!settled)throw std::runtime_error("AMD-only startup failed with saved mode "+std::to_string(saved));
  if(expected!=RunMode::KeyHuntCpu){
   bool visible=false;for(auto* frame:rx.findChildren<QFrame*>())if(auto* controls=dynamic_cast<GpuControlPanel*>(frame))for(auto* label:controls->findChildren<QLabel*>())if(label->text().contains("AMD Radeon RX 5700 XT"))visible=true;
   check(visible,"AMD search device disappeared when power/fan controls were unavailable");
  }
  if(!destination.isEmpty()&&saved==static_cast<int>(RunMode::BitCrackCuda))check(rx.grab().save(destination+"/rx5700xt.png"),"AMD screenshot failed");
  for(auto* p:rx.findChildren<QProcess*>())if(p->state()!=QProcess::NotRunning){p->kill();p->waitForFinished(3000);}
 }
 std::cout<<"Shared Qt panel passed: CUDA/AMD selection, measured profiles, CPU threads, recovery, failed status, translations\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

#include "GpuControlPanel.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QTemporaryDir>
#include <QFile>
#include <QThread>
#include <QTimer>
#include <QMessageBox>
#include <QElapsedTimer>
#include <iostream>
using namespace openpuzzle;using namespace openpuzzle::ui;
void require(bool b,const char* why){if(!b)throw std::runtime_error(why);}
template<class T>T* control(QWidget& widget,const char* name){for(auto* item:widget.findChildren<T*>())if(item->property("controlId").toString()==name)return item;throw std::runtime_error("Control missing");}
int main(int argc,char** argv){QApplication app(argc,argv);try{
 QTemporaryDir dir;require(dir.isValid(),"Temporary fixture directory unavailable");GpuControlDevice n;n.vendor="nvidia";n.pci="00000000:08:00.0";n.uuid="GPU-fixture";n.name="NVIDIA fixture";n.power=n.fan=n.automatic=true;n.powerNow=n.powerDefault=300;n.powerMin=100;n.powerMax=333;n.powerStep=.001;n.fanMode="auto";n.fanNow=40;n.temperature=67;
 auto a=n;a.vendor="amd";a.uuid.clear();a.pci="00000000:09:00.0";a.name="AMD fixture";a.powerUnit="%";a.powerNow=-10;a.powerDefault=0;a.powerMin=-50;a.powerMax=20;a.powerStep=1;
 const auto cli=dir.path()+"/fixture.sh",trace=dir.path()+"/trace";QFile script(cli);require(script.open(QIODevice::WriteOnly),"Fixture script unavailable");
 script.write(("#!/bin/sh\nprintf '%s\\n' \"$*\" >> '"+trace+"'\nif [ \"$1\" = gpu-control ] && [ \"$2\" = list ]; then\ncat <<'END'\n"+QString::fromStdString(formatGpuControls({n,a}))+"END\nexit 0\nfi\nexit 9\n").toUtf8());script.close();script.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner);
 GpuControlPanel panel(cli);panel.setLanguage(UiLanguage::Portuguese);panel.show();auto* gpu=control<QComboBox>(panel,"gpuControlDevice");QElapsedTimer clock;clock.start();
 while(gpu->count()!=2&&clock.elapsed()<4000){app.processEvents();QThread::msleep(5);}require(gpu->count()==2,"Async GPU control inspection failed");
 auto* power=control<QDoubleSpinBox>(panel,"gpuControlPower");require(power->value()==300&&power->suffix()==" W","NVIDIA watt controls incorrect");
 gpu->setCurrentIndex(1);require(power->value()==-10&&power->suffix()==" %"&&power->minimum()==-50,"AMD percent units/range incorrect");
 for(unsigned i=0;i<4;++i){panel.setLanguage(static_cast<UiLanguage>(i));require(power->value()==-10&&gpu->currentIndex()==1,"Language switch changed selected GPU/settings");}
 panel.setLanguage(UiLanguage::Portuguese);QPushButton* apply=nullptr;for(auto* button:panel.findChildren<QPushButton*>())if(button->text()=="Aplicar"){apply=button;break;}require(apply&&apply->isEnabled(),"Translated Apply unavailable");
 QTimer::singleShot(0,[]{for(auto* widget:QApplication::topLevelWidgets())if(auto* dialog=qobject_cast<QMessageBox*>(widget))dialog->done(QMessageBox::No);});apply->click();
 QFile log(trace);require(log.open(QIODevice::ReadOnly),"Fixture trace unavailable");require(log.readAll()=="gpu-control list\n","Inspection or cancelled confirmation called a setter");require(!panel.applying(),"Cancelled change left UI busy");
 std::cout<<"GpuControlPanelTests passed: async inspection, physical GPU selection, watts/percent, four languages and cancellation without writes\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

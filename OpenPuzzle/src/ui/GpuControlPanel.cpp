#include "GpuControlPanel.hpp"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QProcess>
#include <QTimer>
#include <QGridLayout>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QFileInfo>
#ifndef _WIN32
#include <unistd.h>
#endif
namespace openpuzzle::ui {
namespace {QString q(const std::string& s){return QString::fromUtf8(s.data(),static_cast<int>(s.size()));}}
GpuControlPanel::GpuControlPanel(const QString& cli,QWidget* parent):QFrame(parent),cli_(cli){
 setObjectName("Card");auto* layout=new QGridLayout(this);layout->setHorizontalSpacing(12);layout->setVerticalSpacing(10);
 title_=new QLabel;title_->setObjectName("CardTitle");layout->addWidget(title_,0,0,1,5);
 gpu_=new QComboBox;gpu_->setProperty("controlId","gpuControlDevice");refresh_=new QPushButton;
 layout->addWidget(gpu_,1,0,1,4);layout->addWidget(refresh_,1,4);
 info_=new QLabel;info_->setWordWrap(true);info_->setTextInteractionFlags(Qt::TextSelectableByMouse);layout->addWidget(info_,2,0,1,5);
 powerLabel_=new QLabel;power_=new QDoubleSpinBox;power_->setDecimals(3);power_->setProperty("controlId","gpuControlPower");powerApply_=new QPushButton;powerDefault_=new QPushButton;
 layout->addWidget(powerLabel_,3,0);layout->addWidget(power_,3,1);layout->addWidget(powerApply_,3,2);layout->addWidget(powerDefault_,3,3,1,2);
 fanLabel_=new QLabel;mode_=new QComboBox;mode_->setProperty("controlId","gpuControlFanMode");fan_=new QSpinBox;fan_->setRange(30,100);fan_->setSuffix(" %");fan_->setValue(60);fanApply_=new QPushButton;
 layout->addWidget(fanLabel_,4,0);layout->addWidget(mode_,4,1);layout->addWidget(fan_,4,2);layout->addWidget(fanApply_,4,3);
 hint_=new QLabel;hint_->setObjectName("HintText");hint_->setWordWrap(true);layout->addWidget(hint_,5,0,1,5);
 result_=new QLabel;result_->setWordWrap(true);result_->setTextInteractionFlags(Qt::TextSelectableByMouse);layout->addWidget(result_,6,0,1,5);
 reader_=new QProcess(this);writer_=new QProcess(this);timer_=new QTimer(this);readTimeout_=new QTimer(this);readTimeout_->setSingleShot(true);
 connect(readTimeout_,&QTimer::timeout,this,[this]{if(reader_->state()!=QProcess::NotRunning)reader_->kill();});
 connect(reader_,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus status){
  readTimeout_->stop();const auto text=reader_->readAllStandardOutput();
  if(code!=0||status!=QProcess::NormalExit){devices_.clear();info_->setText(t("gpu_failed"));result_->setText(QString::fromUtf8(reader_->readAllStandardError()));updateControls();return;}
  const auto* prior=selected();const auto pci=prior?prior->pci:std::string();
  try{
   auto devices=parseGpuControls(text.toStdString());const QSignalBlocker blocker(gpu_);gpu_->clear();devices_=std::move(devices);int index=0;
   for(std::size_t i=0;i<devices_.size();++i){const auto& d=devices_[i];gpu_->addItem(q(d.name+" / "+d.pci));if(d.pci==pci)index=static_cast<int>(i);}
   gpu_->setCurrentIndex(devices_.empty()?-1:index);render(pci.empty());
  }catch(const std::exception& e){devices_.clear();info_->setText(t("gpu_failed"));result_->setText(q(e.what()));}updateControls();
 });
 connect(reader_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){if(error==QProcess::FailedToStart){devices_.clear();info_->setText(t("gpu_failed"));result_->setText(reader_->errorString());updateControls();}});
 connect(writer_,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus status){
  result_->setText(t(code==0&&status==QProcess::NormalExit?"gpu_applied":"gpu_failed")+"\n"+QString::fromUtf8(writer_->readAllStandardError())+(code?QString::fromUtf8(writer_->readAllStandardOutput()):QString()));refresh();updateControls();
 });
 connect(writer_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){if(error==QProcess::FailedToStart){result_->setText(t("gpu_failed")+"\n"+writer_->errorString());updateControls();}});
 connect(gpu_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this]{render(true);});
 connect(mode_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this]{updateControls();});
 connect(refresh_,&QPushButton::clicked,this,[this]{refresh();});
 connect(powerApply_,&QPushButton::clicked,this,[this]{apply(GpuControlAction::Power);});
 connect(powerDefault_,&QPushButton::clicked,this,[this]{apply(GpuControlAction::PowerDefault);});
 connect(fanApply_,&QPushButton::clicked,this,[this]{apply(mode_->currentIndex()==1?GpuControlAction::FanManual:GpuControlAction::FanAuto);});
 connect(timer_,&QTimer::timeout,this,[this]{refresh();});timer_->start(10000);setLanguage(language_);QTimer::singleShot(0,this,[this]{refresh();});
}
QString GpuControlPanel::t(const char* key)const{return translated(language_,QString::fromLatin1(key));}
bool GpuControlPanel::applying()const{return writer_->state()!=QProcess::NotRunning;}
const GpuControlDevice* GpuControlPanel::selected()const{const auto i=gpu_->currentIndex();return i>=0&&static_cast<std::size_t>(i)<devices_.size()?&devices_[i]:nullptr;}
void GpuControlPanel::setLanguage(UiLanguage language){
 language_=language;title_->setText(t("gpu_controls"));hint_->setText(t("gpu_controls_hint"));powerLabel_->setText(t("gpu_power_limit"));fanLabel_->setText(t("gpu_fan_mode"));
 refresh_->setText(t("refresh"));powerApply_->setText(t("gpu_apply"));powerDefault_->setText(t("gpu_default"));fanApply_->setText(t("gpu_apply"));
 const QSignalBlocker blocker(mode_);const auto index=mode_->currentIndex();mode_->clear();mode_->addItem(t("gpu_fan_auto"));mode_->addItem(t("gpu_fan_manual"));mode_->setCurrentIndex(index==1?1:0);render(false);
}
void GpuControlPanel::updateControls(){
 const bool ready=reader_->state()==QProcess::NotRunning&&!applying();const auto* d=selected();
 gpu_->setEnabled(ready);refresh_->setEnabled(ready);power_->setEnabled(ready&&d&&d->power);powerApply_->setEnabled(ready&&d&&d->power);
 powerDefault_->setEnabled(ready&&d&&d->power&&gpuControlRange(d->powerDefault,d->powerMin,d->powerMax));
 mode_->setEnabled(ready&&d&&d->automatic);const bool manual=mode_->currentIndex()==1;fan_->setEnabled(ready&&d&&d->fan&&manual);fanApply_->setEnabled(ready&&d&&d->automatic&&(!manual||(d->fan&&gpuControlRange(d->temperature,0,79.999))));
}
void GpuControlPanel::render(bool inputs){
 const auto* d=selected();if(!d){info_->setText(t("gpu_no_controls"));updateControls();return;}
 const auto power=d->power?QString::number(d->powerNow)+" "+q(d->powerUnit)+" ("+t("gpu_range")+": "+QString::number(d->powerMin)+"–"+QString::number(d->powerMax)+" "+q(d->powerUnit)+")":t("unavailable");
 info_->setText(q(d->pci+"  "+d->uuid)+"\n"+t("gpu_current")+": "+power+" | "+t("gpu_fan_mode")+": "+t(d->fanMode=="auto"?"gpu_fan_auto":d->fanMode=="manual"?"gpu_fan_manual":"gpu_mixed")+" "+QString::number(d->fanNow,'f',0)+"% | "+(d->temperature>=0?QString::number(d->temperature)+" °C":t("unavailable"))+"\n"+t("gpu_permission"));
 if(inputs){power_->setRange(d->powerMin,d->powerMax);power_->setSingleStep((std::max)(1.0,d->powerStep));power_->setSuffix(" "+q(d->powerUnit));power_->setValue(d->powerNow);fan_->setRange(static_cast<int>(d->fanMin),static_cast<int>(d->fanMax));fan_->setValue(static_cast<int>(std::round(d->fanNow)));mode_->setCurrentIndex(d->fanMode=="manual"?1:0);}updateControls();
}
void GpuControlPanel::refresh(){
 if(reader_->state()!=QProcess::NotRunning||applying())return;reader_->start(cli_,{"gpu-control","list"});readTimeout_->start(15000);updateControls();
}
void GpuControlPanel::apply(GpuControlAction action){
 const auto* d=selected();if(!d||applying()||reader_->state()!=QProcess::NotRunning)return;
 try{
  const auto value=action==GpuControlAction::Power?power_->value():action==GpuControlAction::FanManual?fan_->value():0;
  const auto command=gpuControlArguments(*d,action,value);QStringList args;for(const auto& arg:command)args<<q(arg);
  const auto setting=action==GpuControlAction::Power?t("gpu_power_limit")+": "+QString::number(value)+" "+q(d->powerUnit):action==GpuControlAction::PowerDefault?t("gpu_default")+": "+QString::number(d->powerDefault)+" "+q(d->powerUnit):action==GpuControlAction::FanAuto?t("gpu_fan_auto"):t("gpu_fan_manual")+" "+QString::number(value)+"%";
  if(QMessageBox::question(this,t("gpu_controls"),t("gpu_apply_confirm")+"\n\n"+q(d->name)+"\nPCI: "+q(d->pci)+"\n"+setting,QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
  result_->clear();
#ifndef _WIN32
  if(geteuid()!=0){
   const QFileInfo helper(QStringLiteral("/usr/libexec/openpuzzle-gpu-control"));
   const QFileInfo directory(QStringLiteral("/usr/libexec"));
   const QFileInfo pkexec(QStringLiteral("/usr/bin/pkexec"));
   const auto unsafe=QFileDevice::WriteGroup|QFileDevice::WriteOther;
   if(!helper.isFile()||helper.isSymLink()||helper.ownerId()!=0||(helper.permissions() & unsafe)||directory.ownerId()!=0||(directory.permissions() & unsafe)||!pkexec.isExecutable()||pkexec.ownerId()!=0){
    throw std::runtime_error("Install the dedicated GPU helper with scripts/install-gpu-control-helper.sh before using Apply. No GPU setting changed.");
   }
   writer_->start(pkexec.absoluteFilePath(),QStringList{helper.absoluteFilePath()}+args.mid(1));
  }else
#endif
  writer_->start(cli_,args);updateControls();
 }catch(const std::exception& e){result_->setText(t("gpu_failed")+"\n"+q(e.what()));}
}
}

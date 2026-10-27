#include "VanityPanel.hpp"
#include <QCloseEvent>
#include <QComboBox>
#include <QJsonArray>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
namespace openpuzzle::ui {
namespace {
const char* words[][4]={
 {"Vanity / CUDA — local preview","Vanity / CUDA — preview local","Vanity / CUDA — aperçu local","Vanity / CUDA — vista previa local"},
 {"Measured profile","Perfil medido","Profil mesuré","Perfil medido"},
 {"Import profile…","Importar perfil…","Importer un profil…","Importar perfil…"},
 {"Verify / refresh","Verificar / atualizar","Vérifier / actualiser","Verificar / actualizar"},
 {"Run local test","Executar teste local","Exécuter le test local","Ejecutar prueba local"},
 {"Repeat measurement","Repetir medição","Répéter la mesure","Repetir medición"},
 {"Safe Stop","Paragem segura","Arrêt sûr","Parada segura"},
 {"Stop","Parar","Arrêter","Detener"},
 {"Close","Fechar","Fermer","Cerrar"},
 {"Local test · RTX 4070 SUPER · thermal stop at 85 °C.\nNo server work requested. Independent BitCrack and Vanity profiles.","Teste local · RTX 4070 SUPER · paragem térmica aos 85 °C.\nNão pede trabalho ao servidor. Perfis BitCrack e Vanity independentes.","Test local · RTX 4070 SUPER · arrêt thermique à 85 °C.\nAucune tâche serveur demandée. Profils BitCrack et Vanity indépendants.","Prueba local · RTX 4070 SUPER · parada térmica a 85 °C.\nNo solicita trabajo al servidor. Perfiles BitCrack y Vanity independientes."},
 {"No verified profile selected.","Nenhum perfil verificado selecionado.","Aucun profil vérifié sélectionné.","Ningún perfil verificado seleccionado."},
 {"Stop other GPU searches first. This runs one local assignment (about 30 seconds). Power and fans stay unchanged. Continue?","Para primeiro as outras pesquisas da GPU. Será executada uma atribuição local (cerca de 30 segundos). Potência e ventoinhas mantêm-se. Continuar?","Arrêtez les autres recherches GPU. Une attribution locale sera exécutée (environ 30 secondes). Puissance et ventilateurs inchangés. Continuer ?","Detén las otras búsquedas GPU. Se ejecutará una asignación local (unos 30 segundos). Potencia y ventiladores sin cambios. ¿Continuar?"},
 {"Three paired measurements, about 15–25 minutes. Uses the validated parameters; this does not tune a new grid. Only a successful stable result selects a new independent profile. Continue?","Três pares de medições, cerca de 15–25 minutos. Usa os parâmetros validados; não escolhe uma nova grelha. Só um resultado estável e bem-sucedido seleciona um novo perfil independente. Continuar?","Trois paires de mesures, environ 15–25 minutes. Paramètres validés ; aucune nouvelle grille. Seul un résultat stable et réussi sélectionne un nouveau profil indépendant. Continuer ?","Tres pares de mediciones, unos 15–25 minutos. Parámetros validados; sin elegir otra cuadrícula. Solo un resultado estable y correcto selecciona un nuevo perfil independiente. ¿Continuar?"},
 {"Wait for the owned command to finish, or press Stop.","Aguarda o fim do comando ou carrega em Parar.","Attendez la fin de la commande ou cliquez sur Arrêter.","Espera a que termine el comando o pulsa Detener."},
 {"Stopping; preserving local data…","A parar; a preservar os dados locais…","Arrêt ; conservation des données…","Deteniendo; conservando los datos locales…"},
 {"Safe Stop requested: finish this assignment, then stop.","Paragem segura pedida: terminar esta atribuição e parar.","Arrêt sûr demandé : terminer cette attribution puis arrêter.","Parada segura solicitada: terminar esta asignación y parar."},
 {"Completed","Concluído","Terminé","Completado"},
 {"Cancelled; previous profile preserved.","Cancelado; perfil anterior preservado.","Annulé ; profil précédent conservé.","Cancelado; perfil anterior conservado."},
 {"Command failed; inspect the details below. Existing data preserved.","O comando falhou; consulta os detalhes abaixo. Dados existentes preservados.","Échec de la commande ; consultez les détails. Données existantes conservées.","Falló el comando; consulta los detalles. Datos existentes conservados."},
 {"Median: %1 MKey/s · Planning: %2 MKey/s (−3%)\nGPU: %3\nGrid %4 · Threads %5 · Points %6","Mediana: %1 MKey/s · Planeamento: %2 MKey/s (−3%)\nGPU: %3\nGrelha %4 · Threads %5 · Pontos %6","Médiane : %1 MKey/s · Planification : %2 MKey/s (−3 %)\nGPU : %3\nGrille %4 · Threads %5 · Points %6","Mediana: %1 MKey/s · Planificación: %2 MKey/s (−3%)\nGPU: %3\nCuadrícula %4 · Hilos %5 · Puntos %6"},
 {"Local progress: %1% · %2 MKey/s (since start)","Progresso local: %1% · %2 MKey/s (desde o arranque)","Progression locale : %1 % · %2 MKey/s (depuis le début)","Progreso local: %1% · %2 MKey/s (desde el inicio)"},
 {"Preview storage is unavailable. Open the prepared preview launcher.","Armazenamento do preview indisponível. Usa o lançador do preview preparado.","Stockage indisponible. Utilisez le lanceur de l’aperçu préparé.","Almacenamiento no disponible. Usa el lanzador de la vista previa."},
 {"Resume local run","Retomar execução local","Reprendre l’exécution locale","Reanudar ejecución local"},
 {"Saved local runs","Execuções locais guardadas","Exécutions locales enregistrées","Ejecuciones locales guardadas"},
 {"Resume from %1 of %2 checked keys? Stop other GPU searches first. The same local assignment will continue; no new range will be requested. Power and fans stay unchanged.","Retomar a partir de %1 de %2 chaves verificadas? Para primeiro as outras pesquisas da GPU. A mesma atribuição local continuará; não será pedido outro range. Potência e ventoinhas mantêm-se.","Reprendre à partir de %1 clés sur %2 ? Arrêtez les autres recherches GPU. La même attribution locale continuera, sans nouvelle plage. Puissance et ventilateurs inchangés.","¿Reanudar desde %1 de %2 claves verificadas? Detén las otras búsquedas GPU. Continuará la misma asignación local, sin pedir otro rango. Potencia y ventiladores sin cambios."},
 {"No interrupted local runs available.","Não há execuções locais interrompidas disponíveis.","Aucune exécution locale interrompue disponible.","No hay ejecuciones locales interrumpidas disponibles."},
 {"Ready for a local test","Pronto para um teste local","Prêt pour un test local","Listo para una prueba local"}

};
}
QString VanityPanel::text(int key) const {return QString::fromUtf8(words[key][static_cast<int>(language_)]);}
VanityPanel::VanityPanel(QString cli,UiLanguage language,QWidget* parent,bool embedded):QDialog(parent),cli_(std::move(cli)),language_(language){
 coordinator_=qEnvironmentVariable("OPENPUZZLE_VANITY_COORDINATOR")=="1";
 setObjectName("vanityPanel");setWindowTitle(text(0));resize(760,620);
 auto* layout=new QVBoxLayout(this);hint_=new QLabel(text(9));hint_->setWordWrap(true);hint_->setTextFormat(Qt::PlainText);layout->addWidget(hint_);
 title_=new QLabel(text(1));title_->setObjectName("CardTitle");layout->addWidget(title_);
 profile_=new QLabel(text(10));profile_->setObjectName("vanityProfile");profile_->setWordWrap(true);profile_->setTextFormat(Qt::PlainText);profile_->setMinimumHeight(profile_->fontMetrics().lineSpacing()*6);profile_->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Minimum);layout->addWidget(profile_);
 auto* grid=new QGridLayout;
 auto button=[&](int key,const char* name,int row,int col){auto* b=new QPushButton(text(key));b->setObjectName(name);b->setAutoDefault(false);b->setStyleSheet("QPushButton {border:1px solid #697586;border-radius:5px;padding:8px;} QPushButton:disabled {color:#738092;border-color:#46505d;} QPushButton:hover:enabled {border-color:#d6a437;}");grid->addWidget(b,row,col);return b;};
 import_=button(2,"vanityImport",0,0);verify_=button(3,"vanityVerify",0,1);run_=button(4,"vanityRun",1,0);benchmark_=button(5,"vanityBenchmark",1,1);resume_=button(22,"vanityResume",4,0);safe_=button(6,"vanitySafe",5,0);stop_=button(7,"vanityStop",5,1);layout->addLayout(grid);
 grid->addWidget(resume_,4,0,1,2);runsTitle_=new QLabel(text(23));grid->addWidget(runsTitle_,2,0,1,2);runs_=new QComboBox;runs_->setObjectName("vanityRuns");runs_->setMinimumContentsLength(24);runs_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);grid->addWidget(runs_,3,0,1,2);
 connect(runs_,&QComboBox::currentIndexChanged,this,[this]{availability();});
 connect(resume_,&QPushButton::clicked,this,[this]{auto value=runs_->currentData().toMap();if(!value.value("available").toBool())return;auto path=value.value("path").toString();auto message=text(24).arg(value.value("checked").toLongLong()).arg(value.value("total").toLongLong());if(QMessageBox::question(this,text(0),message,QMessageBox::Yes|QMessageBox::No,QMessageBox::No)==QMessageBox::Yes){resumeState_=value.value("state_sha256").toString();start("measured-resume",path);}});
 status_=new QLabel;status_->setObjectName("vanityStatus");status_->setWordWrap(true);status_->setTextFormat(Qt::PlainText);layout->addWidget(status_);
 output_=new QPlainTextEdit;output_->setObjectName("vanityOutput");output_->setReadOnly(true);output_->setMaximumBlockCount(2000);layout->addWidget(output_);
 close_=new QPushButton(text(8));layout->addWidget(close_);connect(close_,&QPushButton::clicked,this,&VanityPanel::reject);
 store_=qEnvironmentVariable("OPENPUZZLE_VANITY_STORE");
 if(!QFileInfo(store_).isAbsolute()||!QFileInfo(store_).isDir()||QFileInfo(store_).isSymLink())store_.clear();
 connect(import_,&QPushButton::clicked,this,[this]{auto file=QFileDialog::getOpenFileName(this,text(2),{},"JSON (*.json)");if(!file.isEmpty())start("measured-import",file);});
 connect(verify_,&QPushButton::clicked,this,[this]{start("measured-show");});
 connect(run_,&QPushButton::clicked,this,[this]{if(QMessageBox::question(this,text(0),text(11),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)==QMessageBox::Yes)start("measured-test");});
 connect(benchmark_,&QPushButton::clicked,this,[this]{if(QMessageBox::question(this,text(0),text(12),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)==QMessageBox::Yes)start("measured-benchmark");});
 connect(stop_,&QPushButton::clicked,this,&VanityPanel::stop);
 connect(safe_,&QPushButton::clicked,this,[this]{
   if(!process_||(action_!="measured-test"&&action_!="measured-resume")||!QFileInfo(controlsPath()).isDir())return;
   QFile flag(controlsPath()+"/safe-stop");if(flag.open(QIODevice::WriteOnly|QIODevice::NewOnly)){flag.setPermissions(QFile::ReadOwner|QFile::WriteOwner);flag.write("stop\n");flag.close();safeRequested_=true;safe_->setEnabled(false);status(15);availability();}
 });
 if(embedded){setWindowFlags(Qt::Widget);for(auto* b:{run_,benchmark_,resume_,safe_,stop_,close_})b->hide();output_->setMinimumHeight(120);output_->setMaximumHeight(200);}
 availability();if(!store_.isEmpty())QTimer::singleShot(0,this,[this]{start("measured-show");});else status(21);
}
void VanityPanel::availability(){bool idle=!process_&&hostAllowed_,configured=!store_.isEmpty();import_->setEnabled(idle&&configured);verify_->setEnabled(idle&&configured);run_->setEnabled(idle&&ready_);benchmark_->setEnabled(idle&&ready_);stop_->setEnabled(process_&&!cancelled_&&(action_=="measured-test"||action_=="measured-resume"||action_=="measured-benchmark"));safe_->setEnabled(controlsReady_&&process_&&!cancelled_&&!safeRequested_&&(action_=="measured-test"||action_=="measured-resume")&&QFileInfo(controlsPath()).isDir());resume_->setEnabled(idle&&ready_&&runs_->currentData().toMap().value("available").toBool());runs_->setEnabled(idle&&ready_);close_->setEnabled(!process_);if(stateChanged)stateChanged();}
void VanityPanel::start(const QString& action,const QString& candidate){
 if(process_||store_.isEmpty()||(!hostAllowed_&&action!="measured-show"&&action!="measured-runs"))return;
 action_=action;controlsReady_=false;cancelled_=false;safeRequested_=false;pending_.clear();decoder_=QStringDecoder(QStringDecoder::Utf8);
 auto* p=new QProcess(this);process_=p;p->setObjectName("vanityOwnedProcess");p->setProcessChannelMode(QProcess::MergedChannels);p->setStandardInputFile(QProcess::nullDevice());
 QStringList args{"vanity-preview",action,"--store",store_};
 if(action!="measured-show"&&action!="measured-runs")args<<"--enable-experimental-vanity-local";
 if(action=="measured-import")args<<"--from"<<candidate;
 if(action=="measured-test"){runRoot_=QDir(store_).absoluteFilePath("run-"+QUuid::createUuid().toString(QUuid::WithoutBraces));args<<"--run-root"<<runRoot_;}
 if(action=="measured-resume"){runRoot_=candidate;args<<"--run-root"<<runRoot_<<"--expected-state"<<resumeState_;}
 if(coordinator_&&(action=="measured-test"||action=="measured-resume"||action=="measured-runs")){
  const QString route=action=="measured-test"?"start":action=="measured-resume"?"resume":"runs";
  args=QStringList{"run","20","--engine","vanity","--backend","cuda","--vanity-session",route,"--store",store_};
  if(route!="runs")args<<"--enable-experimental-vanity-local"<<"--run-root"<<runRoot_;
  if(route=="resume")args<<"--expected-state"<<resumeState_;
 }
 if(action!="measured-runs"){output_->clear();statusKey_=-1;progressData_={};status_->clear();}availability();
 connect(p,&QProcess::readyReadStandardOutput,this,&VanityPanel::drain);
 connect(p,&QProcess::finished,this,[this,p](int code,QProcess::ExitStatus exit){drain();if(!pending_.isEmpty())output_->appendPlainText(pending_);if(code!=0&&(action_=="measured-show"||action_=="measured-import")){ready_=false;profileData_={};profile_->setText(text(10));}auto finishedAction=action_;if(finishedAction!="measured-runs")status(exit==QProcess::NormalExit&&code==0?((finishedAction=="measured-show"||finishedAction=="measured-import")?26:16):code==130?17:18);else if(code!=0){runs_->clear();status(18);}process_=nullptr;p->deleteLater();if(finishedAction!="measured-runs"&&ready_)start("measured-runs");else availability();});
 connect(p,&QProcess::errorOccurred,this,[this,p](QProcess::ProcessError error){if(error!=QProcess::FailedToStart)return;output_->appendPlainText(p->errorString());status(18);ready_=false;process_=nullptr;p->deleteLater();availability();});
 connect(p,&QProcess::started,this,[this,p]{if(cancelled_)p->terminate();});
 p->start(cli_,args);
}
void VanityPanel::drain(){
 if(!process_)return;
 pending_+=decoder_(process_->readAll());
 while(pending_.contains('\n')){auto i=pending_.indexOf('\n');auto line=pending_.left(i);pending_.remove(0,i+1);auto o=QJsonDocument::fromJson(line.toUtf8()).object();
  if(o["status"]=="ready"){
   profileData_=o;ready_=true;profile_->setText(text(19).arg(o["median_mkeys"].toDouble(),0,'f',2).arg(o["planning_mkeys"].toDouble(),0,'f',2).arg(o["gpu"].toString()).arg(o["grid"].toInt()).arg(o["threads"].toInt()).arg(o["points"].toInt()));
  }else if(o["status"]=="local-runs"){
   auto previous=runs_->currentData().toMap().value("path").toString();runs_->clear();
   for(const auto& item:o["runs"].toArray()){auto entry=item.toObject();auto path=entry["path"].toString();if(!entry["available"].toBool())continue;if(QFileInfo(path).absolutePath()!=QDir(store_).absolutePath())continue;auto label=QFileInfo(path).fileName()+" — "+QString::number(entry["checked"].toDouble()/entry["total"].toDouble()*100,'f',1)+"%";runs_->addItem(label,entry.toVariantMap());if(path==previous||path==runRoot_)runs_->setCurrentIndex(runs_->count()-1);}
   if(runs_->count()==0)runs_->addItem(text(25));
  }else if(o["status"]=="running"&&o["total"].toDouble()>0){controlsReady_=true;progressData_=o;if(!cancelled_){statusKey_=-1;status_->setText(text(20).arg(o["checked"].toDouble()/o["total"].toDouble()*100,0,'f',1).arg(o["speed_mkeys"].toDouble(),0,'f',2));}}
  else output_->appendPlainText(line);
  availability();
 }
}
QString VanityPanel::controlsPath() const {return runRoot_+(coordinator_?"/native/controls":"/controls");}
bool VanityPanel::writeCancel(){
 if(!controlsReady_||(action_!="measured-test"&&action_!="measured-resume"))return false;
 const QFileInfo root(runRoot_),controls(controlsPath());
 if(root.absolutePath()!=QDir(store_).absolutePath()||root.isSymLink()||!root.isDir()||root.canonicalFilePath()!=root.absoluteFilePath()
    ||controls.isSymLink()||!controls.isDir()||controls.canonicalFilePath()!=controls.absoluteFilePath())return false;
 // A completed seal must remain immutable even if the final process-exit
 // notification has not reached the UI yet.
 const QString seal=runRoot_+(coordinator_?"/coordinator-seal.json":"/resume-state.json");
 const QString active=runRoot_+(coordinator_?"/coordinator-active.json":"/resume-active.json");
 if(QFileInfo::exists(seal)&&!QFileInfo::exists(active))return false;
 const QString name=controlsPath()+"/cancel";QFileInfo info(name);
 if(info.isSymLink())return false;
 if(info.exists())return info.isFile()&&info.size()==0;
 QFile flag(name);if(!flag.open(QIODevice::WriteOnly|QIODevice::NewOnly))return false;
 return flag.setPermissions(QFile::ReadOwner|QFile::WriteOwner)&&flag.flush();
}
void VanityPanel::controlEvent(const QString& event,bool marker){
 if(store_.isEmpty())return;
 const QString path=store_+"/ui-controls.jsonl";QFileInfo info(path);
 if(info.isSymLink()||(info.exists()&&!info.isFile()))return;
 QFile log(path);if(!log.open(QIODevice::WriteOnly|QIODevice::Append))return;
 if(!log.setPermissions(QFile::ReadOwner|QFile::WriteOwner))return;
 QJsonObject record{{"event",event},{"utc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
  {"action",action_},{"run_root",runRoot_},{"cancel_marker",marker},
  {"helper_pid",process_?process_->processId():0},{"checked",progressData_["checked"]}};
 log.write(QJsonDocument(record).toJson(QJsonDocument::Compact)+"\n");log.flush();
}
void VanityPanel::stop(){
 if(!process_||cancelled_)return;
 const bool marker=writeCancel();controlEvent("stop-requested",marker);
 cancelled_=true;status(14);
 if(process_->state()==QProcess::Running)process_->terminate();
 availability();
}
void VanityPanel::reject(){if(process_){status(13);return;}QDialog::reject();}
void VanityPanel::closeEvent(QCloseEvent* event){if(process_){status(13);event->ignore();}else event->accept();}
void VanityPanel::status(int key){statusKey_=key;progressData_={};status_->setText(text(key));}
void VanityPanel::setLanguage(UiLanguage language){
 language_=language;setWindowTitle(text(0));hint_->setText(text(9));title_->setText(text(1));runsTitle_->setText(text(23));
 for(auto pair:{std::pair{import_,2},{verify_,3},{run_,4},{benchmark_,5},{safe_,6},{stop_,7},{close_,8},{resume_,22}})pair.first->setText(text(pair.second));
 const auto& o=profileData_;
 profile_->setText(o.isEmpty()?text(10):text(19).arg(o["median_mkeys"].toDouble(),0,'f',2).arg(o["planning_mkeys"].toDouble(),0,'f',2).arg(o["gpu"].toString()).arg(o["grid"].toInt()).arg(o["threads"].toInt()).arg(o["points"].toInt()));
 if(statusKey_>=0)status_->setText(text(statusKey_));
 else if(!progressData_.isEmpty()){auto p=progressData_;status_->setText(text(20).arg(p["checked"].toDouble()/p["total"].toDouble()*100,0,'f',1).arg(p["speed_mkeys"].toDouble(),0,'f',2));}
 if(runs_->count()==1&&!runs_->itemData(0).isValid())runs_->setItemText(0,text(25));
}
void VanityPanel::setHostAllowed(bool allowed){if(hostAllowed_==allowed)return;hostAllowed_=allowed;availability();}
bool VanityPanel::canRun() const{return run_->isEnabled();}
bool VanityPanel::canResume() const{return resume_->isEnabled();}
bool VanityPanel::canBenchmark() const{return benchmark_->isEnabled();}
bool VanityPanel::canStop() const{return stop_->isEnabled();}
bool VanityPanel::canSafeStop() const{return safe_->isEnabled();}
void VanityPanel::requestRun(){run_->click();}
void VanityPanel::requestResume(){resume_->click();}
void VanityPanel::requestBenchmark(){benchmark_->click();}
void VanityPanel::requestStop(){stop_->click();}
void VanityPanel::requestSafeStop(){safe_->click();}
QString VanityPanel::statusText() const{return status_->text();}

}

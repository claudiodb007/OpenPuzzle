#include "VanityPanel.hpp"
#include <QCloseEvent>
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
 {"RTX 4070 SUPER only. Local synthetic assignment; no production work. The measured engine and profile remain separate from BitCrack.","Apenas RTX 4070 SUPER. Atribuição sintética local; sem pesquisa de produção. O motor e o perfil medidos são independentes do BitCrack.","RTX 4070 SUPER uniquement. Attribution synthétique locale ; aucune tâche de production. Moteur et profil indépendants de BitCrack.","Solo RTX 4070 SUPER. Asignación sintética local; sin trabajo de producción. Motor y perfil independientes de BitCrack."},
 {"No verified profile selected.","Nenhum perfil verificado selecionado.","Aucun profil vérifié sélectionné.","Ningún perfil verificado seleccionado."},
 {"Stop other GPU searches first. This runs one local assignment (about 30 seconds). Power and fans stay unchanged. Continue?","Para primeiro as outras pesquisas da GPU. Será executada uma atribuição local (cerca de 30 segundos). Potência e ventoinhas mantêm-se. Continuar?","Arrêtez les autres recherches GPU. Une attribution locale sera exécutée (environ 30 secondes). Puissance et ventilateurs inchangés. Continuer ?","Detén las otras búsquedas GPU. Se ejecutará una asignación local (unos 30 segundos). Potencia y ventiladores sin cambios. ¿Continuar?"},
 {"Three paired measurements, about 15–25 minutes. Uses the validated parameters; this does not tune a new grid. Only a successful stable result selects a new independent profile. Continue?","Três pares de medições, cerca de 15–25 minutos. Usa os parâmetros validados; não escolhe uma nova grelha. Só um resultado estável e bem-sucedido seleciona um novo perfil independente. Continuar?","Trois paires de mesures, environ 15–25 minutes. Paramètres validés ; aucune nouvelle grille. Seul un résultat stable et réussi sélectionne un nouveau profil indépendant. Continuer ?","Tres pares de mediciones, unos 15–25 minutos. Parámetros validados; sin elegir otra cuadrícula. Solo un resultado estable y correcto selecciona un nuevo perfil independiente. ¿Continuar?"},
 {"Wait for the owned command to finish, or press Stop.","Aguarda o fim do comando ou carrega em Parar.","Attendez la fin de la commande ou cliquez sur Arrêter.","Espera a que termine el comando o pulsa Detener."},
 {"Stopping; preserving local data…","A parar; a preservar os dados locais…","Arrêt ; conservation des données…","Deteniendo; conservando los datos locales…"},
 {"Safe Stop requested: finish this assignment, then stop.","Paragem segura pedida: terminar esta atribuição e parar.","Arrêt sûr demandé : terminer cette attribution puis arrêter.","Parada segura solicitada: terminar esta asignación y parar."},
 {"Completed","Concluído","Terminé","Completado"},
 {"Cancelled; previous profile preserved.","Cancelado; perfil anterior preservado.","Annulé ; profil précédent conservé.","Cancelado; perfil anterior conservado."},
 {"Command failed; inspect the details below. Existing data preserved.","O comando falhou; consulta os detalhes abaixo. Dados existentes preservados.","Échec de la commande ; consultez les détails. Données existantes conservées.","Falló el comando; consulta los detalles. Datos existentes conservados."},
 {"Median: %1 MKey/s · Planning: %2 MKey/s (−3%)\nGPU: %3\nGrid %4 · Threads %5 · Points %6\nMeasured through the prior CLI; production disabled.","Mediana: %1 MKey/s · Planeamento: %2 MKey/s (−3%)\nGPU: %3\nGrelha %4 · Threads %5 · Pontos %6\nMedido através da CLI anterior; produção desativada.","Médiane : %1 MKey/s · Planification : %2 MKey/s (−3 %)\nGPU : %3\nGrille %4 · Threads %5 · Points %6\nMesuré avec la CLI précédente ; production désactivée.","Mediana: %1 MKey/s · Planificación: %2 MKey/s (−3%)\nGPU: %3\nCuadrícula %4 · Hilos %5 · Puntos %6\nMedido con la CLI anterior; producción desactivada."},
 {"Local progress: %1% · %2 MKey/s (since start)","Progresso local: %1% · %2 MKey/s (desde o arranque)","Progression locale : %1 % · %2 MKey/s (depuis le début)","Progreso local: %1% · %2 MKey/s (desde el inicio)"},
 {"Preview storage is unavailable. Open the prepared preview launcher.","Armazenamento do preview indisponível. Usa o lançador do preview preparado.","Stockage indisponible. Utilisez le lanceur de l’aperçu préparé.","Almacenamiento no disponible. Usa el lanzador de la vista previa."}
};
}
QString VanityPanel::text(int key) const {return QString::fromUtf8(words[key][static_cast<int>(language_)]);}
VanityPanel::VanityPanel(QString cli,UiLanguage language,QWidget* parent):QDialog(parent),cli_(std::move(cli)),language_(language){
 setObjectName("vanityPanel");setWindowTitle(text(0));resize(760,620);
 auto* layout=new QVBoxLayout(this);auto* hint=new QLabel(text(9));hint->setWordWrap(true);layout->addWidget(hint);
 auto* title=new QLabel(text(1));title->setObjectName("CardTitle");layout->addWidget(title);
 profile_=new QLabel(text(10));profile_->setObjectName("vanityProfile");profile_->setWordWrap(true);profile_->setTextFormat(Qt::PlainText);profile_->setMinimumHeight(profile_->fontMetrics().lineSpacing()*6);profile_->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Minimum);layout->addWidget(profile_);
 auto* grid=new QGridLayout;
 auto button=[&](int key,const char* name,int row,int col){auto* b=new QPushButton(text(key));b->setObjectName(name);b->setAutoDefault(false);b->setStyleSheet("QPushButton {border:1px solid #697586;border-radius:5px;padding:8px;} QPushButton:disabled {color:#738092;border-color:#46505d;} QPushButton:hover:enabled {border-color:#d6a437;}");grid->addWidget(b,row,col);return b;};
 import_=button(2,"vanityImport",0,0);verify_=button(3,"vanityVerify",0,1);run_=button(4,"vanityRun",1,0);benchmark_=button(5,"vanityBenchmark",1,1);safe_=button(6,"vanitySafe",2,0);stop_=button(7,"vanityStop",2,1);layout->addLayout(grid);
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
   if(!process_||action_!="measured-test"||!QFileInfo(runRoot_+"/controls").isDir())return;
   QFile flag(runRoot_+"/controls/safe-stop");if(flag.open(QIODevice::WriteOnly|QIODevice::NewOnly)){flag.setPermissions(QFile::ReadOwner|QFile::WriteOwner);flag.write("stop\n");flag.close();safeRequested_=true;safe_->setEnabled(false);status_->setText(text(15));}
 });
 availability();if(!store_.isEmpty())QTimer::singleShot(0,this,[this]{start("measured-show");});else status_->setText(text(21));
}
void VanityPanel::availability(){bool idle=!process_,configured=!store_.isEmpty();import_->setEnabled(idle&&configured);verify_->setEnabled(idle&&configured);run_->setEnabled(idle&&ready_);benchmark_->setEnabled(idle&&ready_);stop_->setEnabled(!idle&&!cancelled_&&(action_=="measured-test"||action_=="measured-benchmark"));safe_->setEnabled(!idle&&!cancelled_&&!safeRequested_&&action_=="measured-test"&&QFileInfo(runRoot_+"/controls").isDir());close_->setEnabled(idle);}
void VanityPanel::start(const QString& action,const QString& candidate){
 if(process_||store_.isEmpty())return;
 action_=action;cancelled_=false;safeRequested_=false;pending_.clear();decoder_=QStringDecoder(QStringDecoder::Utf8);
 auto* p=new QProcess(this);process_=p;p->setObjectName("vanityOwnedProcess");p->setProcessChannelMode(QProcess::MergedChannels);p->setStandardInputFile(QProcess::nullDevice());
 QStringList args{"vanity-preview",action,"--store",store_};
 if(action!="measured-show")args<<"--enable-experimental-vanity-local";
 if(action=="measured-import")args<<"--from"<<candidate;
 if(action=="measured-test"){runRoot_=QDir(store_).absoluteFilePath("run-"+QUuid::createUuid().toString(QUuid::WithoutBraces));args<<"--run-root"<<runRoot_;}
 output_->clear();status_->clear();availability();
 connect(p,&QProcess::readyReadStandardOutput,this,&VanityPanel::drain);
 connect(p,&QProcess::finished,this,[this,p](int code,QProcess::ExitStatus exit){drain();if(!pending_.isEmpty())output_->appendPlainText(pending_);if(code!=0&&(action_=="measured-show"||action_=="measured-import")){ready_=false;profile_->setText(text(10));}status_->setText(text(exit==QProcess::NormalExit&&code==0?16:code==130?17:18));process_=nullptr;p->deleteLater();availability();});
 connect(p,&QProcess::errorOccurred,this,[this,p](QProcess::ProcessError error){if(error!=QProcess::FailedToStart)return;output_->appendPlainText(p->errorString());status_->setText(text(18));ready_=false;process_=nullptr;p->deleteLater();availability();});
 connect(p,&QProcess::started,this,[this,p]{if(cancelled_)p->terminate();});
 p->start(cli_,args);
}
void VanityPanel::drain(){
 if(!process_)return;
 pending_+=decoder_(process_->readAll());
 while(pending_.contains('\n')){auto i=pending_.indexOf('\n');auto line=pending_.left(i);pending_.remove(0,i+1);auto o=QJsonDocument::fromJson(line.toUtf8()).object();
  if(o["status"]=="ready"){
   ready_=true;profile_->setText(text(19).arg(o["median_mkeys"].toDouble(),0,'f',2).arg(o["planning_mkeys"].toDouble(),0,'f',2).arg(o["gpu"].toString()).arg(o["grid"].toInt()).arg(o["threads"].toInt()).arg(o["points"].toInt()));
  }else if(o["status"]=="running"&&o["total"].toDouble()>0){status_->setText(text(20).arg(o["checked"].toDouble()/o["total"].toDouble()*100,0,'f',1).arg(o["speed_mkeys"].toDouble(),0,'f',2));}
  else output_->appendPlainText(line);
  availability();
 }
}
void VanityPanel::stop(){if(!process_||cancelled_)return;cancelled_=true;status_->setText(text(14));if(process_->state()==QProcess::Running)process_->terminate();availability();}
void VanityPanel::reject(){if(process_){status_->setText(text(13));return;}QDialog::reject();}
void VanityPanel::closeEvent(QCloseEvent* event){if(process_){status_->setText(text(13));event->ignore();}else event->accept();}
}

#pragma once
#include "UiLanguage.hpp"
#include <QDialog>
#include <QStringDecoder>
#include <functional>
#include <QJsonObject>
class QComboBox; class QLabel; class QPushButton; class QPlainTextEdit; class QProcess;
namespace openpuzzle::ui {
class VanityPanel final : public QDialog {
public:
 explicit VanityPanel(QString cli, UiLanguage language, QWidget* parent=nullptr, bool embedded=false);
 void setLanguage(UiLanguage);
 void setHostAllowed(bool);
 bool busy() const {return process_!=nullptr;}
 bool canRun() const; bool canResume() const; bool canBenchmark() const;
 bool canStop() const; bool canSafeStop() const;
 void requestRun(); void requestResume(); void requestBenchmark();
 void requestStop(); void requestSafeStop();
 QString statusText() const;
 QString caption(int key) const {return text(key);}
 std::function<void()> stateChanged;
protected:
 void reject() override;
 void closeEvent(QCloseEvent*) override;
private:
 QString text(int key) const;
 void start(const QString& action,const QString& candidate={});
 void availability();
 void drain();
 void stop();
 bool writeCancel();
 QString controlsPath() const;
 bool coordinator_=false;
 void controlEvent(const QString& event,bool marker);
 QString cli_,store_,runRoot_,action_,resumeState_;
 QComboBox* runs_;
 QLabel *hint_,*title_,*runsTitle_;
 QJsonObject profileData_,progressData_;
 int statusKey_=-1;
 bool hostAllowed_=true;
 void status(int);
 UiLanguage language_;
 QLabel* profile_; QLabel* status_;
 QPushButton *import_,*verify_,*run_,*benchmark_,*resume_,*safe_,*stop_,*close_;
 QPlainTextEdit* output_;
 QProcess* process_=nullptr;
 QStringDecoder decoder_{QStringDecoder::Utf8};
 QString pending_;
 bool ready_=false,cancelled_=false,safeRequested_=false,controlsReady_=false;
};
}

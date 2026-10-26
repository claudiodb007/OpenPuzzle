#pragma once
#include "UiLanguage.hpp"
#include <QDialog>
#include <QStringDecoder>
class QLabel; class QPushButton; class QPlainTextEdit; class QProcess;
namespace openpuzzle::ui {
class VanityPanel final : public QDialog {
public:
 explicit VanityPanel(QString cli, UiLanguage language, QWidget* parent=nullptr);
protected:
 void reject() override;
 void closeEvent(QCloseEvent*) override;
private:
 QString text(int key) const;
 void start(const QString& action,const QString& candidate={});
 void availability();
 void drain();
 void stop();
 QString cli_,store_,runRoot_,action_;
 UiLanguage language_;
 QLabel* profile_; QLabel* status_;
 QPushButton *import_,*verify_,*run_,*benchmark_,*safe_,*stop_,*close_;
 QPlainTextEdit* output_;
 QProcess* process_=nullptr;
 QStringDecoder decoder_{QStringDecoder::Utf8};
 QString pending_;
 bool ready_=false,cancelled_=false,safeRequested_=false;
};
}

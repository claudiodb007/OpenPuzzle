#pragma once
#include "UiLanguage.hpp"
#include "openpuzzle/ui/GpuControlUiModel.hpp"
#include <QFrame>
#include <QString>
#include <QStringList>
#ifdef _WIN32
#include "openpuzzle/ui/WindowsUiProcess.hpp"
#endif
class QComboBox;class QDoubleSpinBox;class QSpinBox;class QLabel;class QPushButton;class QProcess;class QTimer;
namespace openpuzzle::ui {
class GpuControlPanel final:public QFrame {
public:
 explicit GpuControlPanel(const QString& cli,QWidget* parent=nullptr);
 void setLanguage(UiLanguage language);
 void setDetectedGpus(const QStringList& names);
 bool applying()const;
private:
 QString t(const char* key)const;
 QString detectedPrefix()const;
 void refresh();void render(bool inputs);void updateControls();void apply(GpuControlAction action);
 const GpuControlDevice* selected()const;
 QString cli_;UiLanguage language_=UiLanguage::English;
 std::vector<GpuControlDevice> devices_;QStringList detectedGpus_;
 QLabel *title_,*info_,*hint_,*powerLabel_,*fanLabel_,*result_;
 QComboBox *gpu_,*mode_;QDoubleSpinBox* power_;QSpinBox* fan_;
 QPushButton *refresh_,*powerApply_,*powerDefault_,*fanApply_;
#ifdef _WIN32
 WindowsUiProcess elevatedWriter_;
#endif
 QProcess *reader_,*writer_;QTimer *timer_,*readTimeout_;
};
}

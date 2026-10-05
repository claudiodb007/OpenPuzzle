#pragma once

#include "UiLanguage.hpp"
#include "UiTheme.hpp"
#include "ThermalAlerts.hpp"
#include "ThermalHistory.hpp"
#include "openpuzzle/ui/RunCommandBuilder.hpp"

#include <QMainWindow>
#include <QStringList>

class QComboBox;
class QCheckBox;
class QDoubleSpinBox;
class QFrame;
class QHBoxLayout;
class QLabel;
class QPlainTextEdit;
class QProcess;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QTimer;
class QWidget;

namespace openpuzzle::ui {

class MainWindow final : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

private:
  void buildInterface();
  void applyLanguage();
  void applyTheme();
  void loadThermalSettings();
  void updateThermalControlState();
  void saveThermalSettings();
  void updateSelectionRules();
  void updateActionAvailability();
  void refreshStatus();
  void handleStatusResult(int exitCode, bool normalExit);
  void markStatusUnavailable(
      const QString& reasonKey,
      const QString& details = QString());
  void updateStatusSummary();
  void updateStatusBadge();
  void refreshRuntimeLog();
  void showSolutionNotice(const QString& assignmentId);
  void configureAutomaticStart(bool enabled);
  bool writeAutomaticStartUnit(QString& error);
  bool runSystemctl(
      const QStringList& arguments,
      QString& error) const;
  QString automaticStartUnitPath() const;
  QString runtimeLogPath() const;
  QString systemctlExecutable() const;
  void rebuildSlotCards(const QString& statusOutput);
  void updateThermalAlerts(
      const QString& statusOutput,
      bool announceTransitions = true);
  void refreshThermalHistory();
  void confirmClearThermalHistory();
  void startExecution();
  void runCommand(
      const QString& titleKey,
      const QStringList& arguments);
  void runSelfTest();
  void confirmStop();
  void confirmKangarooInstall();
  void releaseCommand(QProcess* process);
  QString cliExecutable() const;
  void showOutput(const QString& title, const QString& output);
  QString t(const QString& key) const;
  RunMode selectedRunMode() const;

  QLabel* subtitle_ = nullptr;
  QLabel* currentStateTitle_ = nullptr;
  QLabel* newExecutionTitle_ = nullptr;
  QLabel* toolsTitle_ = nullptr;
  QLabel* thermalTitle_ = nullptr;
  QLabel* thermalHint_ = nullptr;
  QLabel* thermalWarningName_ = nullptr;
  QLabel* thermalCriticalName_ = nullptr;
  QLabel* detailsTitle_ = nullptr;
  QLabel* statusBadge_ = nullptr;
  QFrame* thermalAlertBanner_ = nullptr;
  QLabel* thermalAlertTitle_ = nullptr;
  QLabel* thermalAlertMessage_ = nullptr;
  QLabel* summary_ = nullptr;
  QLabel* puzzleName_ = nullptr;
  QLabel* modeName_ = nullptr;
  QLabel* deviceName_ = nullptr;
  QLabel* openclDeviceName_ = nullptr;
  QLabel* cpuThreadsName_ = nullptr;
  QWidget* slotsHost_ = nullptr;
  QHBoxLayout* slotsLayout_ = nullptr;
  QSpinBox* puzzle_ = nullptr;
  QComboBox* kangarooPuzzle_ = nullptr;
  QComboBox* mode_ = nullptr;
  QComboBox* language_ = nullptr;
  QComboBox* theme_ = nullptr;
  QSpinBox* device_ = nullptr;
  QSpinBox* openclDevice_ = nullptr;
  QSpinBox* cpuThreads_ = nullptr;
  QCheckBox* rusticlRadeonsi_ = nullptr;
  QCheckBox* autoStart_ = nullptr;
  QCheckBox* thermalEnabled_ = nullptr;
  QCheckBox* thermalStopOnCritical_ = nullptr;
  QDoubleSpinBox* thermalWarning_ = nullptr;
  QDoubleSpinBox* thermalCritical_ = nullptr;
  QPushButton* saveThermal_ = nullptr;
  QPushButton* start_ = nullptr;
  QPushButton* safeStop_ = nullptr;
  QPushButton* stop_ = nullptr;
  QPushButton* refresh_ = nullptr;
  QPushButton* benchmark_ = nullptr;
  QPushButton* selfTest_ = nullptr;
  QPushButton* doctor_ = nullptr;
  QPushButton* updates_ = nullptr;
  QPushButton* audit_ = nullptr;
  QPushButton* installKangaroo_ = nullptr;
  QPlainTextEdit* output_ = nullptr;
  QPlainTextEdit* statusOutput_ = nullptr;
  QPlainTextEdit* runtimeLogOutput_ = nullptr;
  QPlainTextEdit* thermalHistoryOutput_ = nullptr;
  QPushButton* clearThermalHistory_ = nullptr;
  QTabWidget* detailsTabs_ = nullptr;
  QProcess* statusProcess_ = nullptr;
  QProcess* commandProcess_ = nullptr;
  QTimer* refreshTimer_ = nullptr;
  QTimer* statusTimeout_ = nullptr;
  bool active_ = false;
  bool identityUnavailable_ = false;
  bool statusAvailable_ = false;
  bool statusTimedOut_ = false;
  bool statusProcessError_ = false;
  QString statusIssueKey_ = QStringLiteral("consulting");
  bool solutionFound_ = false;
  bool kangarooActive_ = false;
  bool launchPending_ = false;
  bool cliAvailable_ = true;
  QString lastSolutionNoticeId_;
  ThermalAlertTracker thermalAlertTracker_;
  ThermalHistoryStore thermalHistory_;
  UiLanguage currentLanguage_ = UiLanguage::English;
  UiTheme currentTheme_ = UiTheme::Light;
};

} // namespace openpuzzle::ui

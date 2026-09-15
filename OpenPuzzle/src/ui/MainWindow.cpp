#include "MainWindow.hpp"

#include "openpuzzle/ui/RunCommandBuilder.hpp"

#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollArea>
#include <QScrollBar>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStandardPaths>
#include <QSettings>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QStyle>
#include <QStringList>
#include <QTabWidget>
#include <QTimer>
#include <QThread>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

#include <exception>

namespace openpuzzle::ui {

namespace {

struct RuntimeSlotStatus {
  QHash<QString, QString> fields;
};

QVector<RuntimeSlotStatus> parseRuntimeSlots(
    const QString& output) {
  static const QRegularExpression fieldPattern(
      R"(^(.+?)\.{2,}\s*(.*)$)");

  QVector<RuntimeSlotStatus> runtimeSlots;
  RuntimeSlotStatus current;

  for (const QString& line : output.split('\n')) {
    const auto match = fieldPattern.match(line.trimmed());
    if (!match.hasMatch()) {
      continue;
    }

    const QString key = match.captured(1).trimmed();
    const QString value = match.captured(2).trimmed();

    if (key == "Slot" && !current.fields.isEmpty()) {
      runtimeSlots.push_back(current);
      current.fields.clear();
    }

    current.fields.insert(key, value);
  }

  if (!current.fields.isEmpty()) {
    runtimeSlots.push_back(current);
  }

  return runtimeSlots;
}

QString statusField(
    const RuntimeSlotStatus& slot,
    const QString& key) {
  const QString value = slot.fields.value(key);
  return value.isEmpty() ? QStringLiteral("—") : value;
}

QString systemdQuote(QString value) {
  value.replace("%", "%%");
  value.replace("\\", "\\\\");
  value.replace("\"", "\\\"");
  return "\"" + value + "\"";
}

void setTextPreservingScroll(
    QPlainTextEdit* editor,
    const QString& text) {
  if (editor->toPlainText() == text) {
    return;
  }

  auto* scrollBar = editor->verticalScrollBar();
  const bool wasEmpty = editor->toPlainText().isEmpty();
  const int previousPosition = scrollBar->value();
  const bool followedEnd =
      previousPosition >= scrollBar->maximum() - 2;

  editor->setPlainText(text);
  if (wasEmpty) {
    scrollBar->setValue(0);
  } else if (followedEnd) {
    scrollBar->setValue(scrollBar->maximum());
  } else {
    scrollBar->setValue(
        qMin(previousPosition, scrollBar->maximum()));
  }
}

QLabel* formLabel(const QString& text) {
  auto* label = new QLabel(text);
  label->setObjectName("FormLabel");
  return label;
}

QFrame* createMetric(
    const QString& nameText,
    const QString& valueText) {
  auto* frame = new QFrame;
  frame->setObjectName("Metric");

  auto* layout = new QVBoxLayout(frame);
  layout->setContentsMargins(12, 9, 12, 9);
  layout->setSpacing(3);

  auto* name = new QLabel(nameText);
  name->setObjectName("MetricName");

  auto* value = new QLabel(valueText);
  value->setObjectName("MetricValue");
  value->setTextInteractionFlags(
      Qt::TextSelectableByMouse);
  value->setWordWrap(true);

  layout->addWidget(name);
  layout->addWidget(value);
  return frame;
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      statusProcess_(new QProcess(this)),
      refreshTimer_(new QTimer(this)),
      statusTimeout_(new QTimer(this)) {
  QSettings settings;
  currentLanguage_ = languageFromCode(
      settings.value("ui/language", "en").toString());
  currentTheme_ = themeFromCode(
      settings.value("ui/theme", "light").toString());

  buildInterface();

  const int savedMode = settings.value(
      "run/mode",
      static_cast<int>(RunMode::BitCrackCuda)).toInt();
  const int savedModeIndex = mode_->findData(savedMode);
  if (savedModeIndex >= 0) {
    mode_->setCurrentIndex(savedModeIndex);
  }
  puzzle_->setValue(settings.value("run/puzzle", 71).toInt());
  const int savedKangarooPuzzle = settings.value(
      "run/kangaroo_puzzle", 140).toInt();
  const int savedKangarooIndex =
      kangarooPuzzle_->findData(savedKangarooPuzzle);
  if (savedKangarooIndex >= 0) {
    kangarooPuzzle_->setCurrentIndex(savedKangarooIndex);
  }
  device_->setValue(settings.value("run/device", 0).toInt());
  openclDevice_->setValue(
      settings.value("run/opencl_device", 1).toInt());
  cpuThreads_->setValue(
      settings.value(
          "run/cpu_threads",
          qMax(1, QThread::idealThreadCount() - 1)).toInt());
  rusticlRadeonsi_->setChecked(
      settings.value("run/rusticl_radeonsi", false).toBool());
  {
    const QSignalBlocker blocker(autoStart_);
    autoStart_->setChecked(
        settings.value("run/auto_start", false).toBool());
  }
  updateSelectionRules();

  statusProcess_->setProcessChannelMode(
      QProcess::MergedChannels);

  connect(
      statusProcess_,
      &QProcess::finished,
      this,
      [this](int, QProcess::ExitStatus) {
        statusTimeout_->stop();
        handleStatusResult();
      });

  connect(
      statusProcess_,
      &QProcess::errorOccurred,
      this,
      [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) {
          return;
        }

        statusTimeout_->stop();
        updateStatusBadge();
        summary_->setText(t("control_failed"));
        cliAvailable_ = false;
        updateActionAvailability();
      });

  statusTimeout_->setSingleShot(true);
  statusTimeout_->setInterval(10000);

  connect(
      statusTimeout_,
      &QTimer::timeout,
      this,
      [this]() {
        if (statusProcess_->state() != QProcess::NotRunning) {
          statusProcess_->kill();
          updateStatusBadge();
          summary_->setText(t("status_timeout"));
        }
      });

  refreshTimer_->setInterval(3000);
  connect(
      refreshTimer_,
      &QTimer::timeout,
      this,
      &MainWindow::refreshStatus);
  refreshTimer_->start();

  applyLanguage();
  applyTheme();
  refreshStatus();
}

void MainWindow::buildInterface() {
  setWindowTitle("OpenPuzzle");
  setMinimumSize(820, 620);
  resize(1120, 860);

  auto* central = new QWidget(this);
  central->setObjectName("PageBackground");
  auto* shell = new QHBoxLayout(central);
  shell->setContentsMargins(20, 0, 20, 0);
  shell->addStretch(1);

  auto* content = new QWidget;
  content->setObjectName("PageContent");
  content->setMinimumWidth(760);
  content->setMaximumWidth(1240);
  content->setSizePolicy(
      QSizePolicy::Expanding,
      QSizePolicy::Expanding);

  auto* page = new QVBoxLayout(content);
  page->setContentsMargins(28, 24, 28, 24);
  page->setSpacing(18);

  shell->addWidget(content, 10);
  shell->addStretch(1);

  auto* header = new QHBoxLayout;
  auto* logo = new QLabel;
  logo->setObjectName("LogoPlate");
  logo->setAlignment(Qt::AlignCenter);
  logo->setFixedSize(164, 92);
  logo->setPixmap(
      QPixmap(":/branding/openpuzzle-logo-header.png")
          .scaled(
              154,
              82,
              Qt::KeepAspectRatio,
              Qt::SmoothTransformation));
  header->addWidget(logo);

  auto* identity = new QVBoxLayout;
  subtitle_ = new QLabel;
  subtitle_->setObjectName("Subtitle");
  subtitle_->setWordWrap(true);
  identity->addWidget(subtitle_);
  header->addLayout(identity);
  header->addStretch();

  auto* selectors = new QGridLayout;
  auto* languageName = formLabel(QString());
  languageName->setProperty("translationKey", "language");
  auto* themeName = formLabel(QString());
  themeName->setProperty("translationKey", "theme");

  language_ = new QComboBox;
  language_->addItem("English", "en");
  language_->addItem("Português", "pt");
  language_->addItem("Français", "fr");
  language_->addItem("Español", "es");
  language_->setCurrentIndex(
      language_->findData(languageCode(currentLanguage_)));

  theme_ = new QComboBox;
  theme_->addItem(QString(), "light");
  theme_->addItem(QString(), "dark");
  theme_->setCurrentIndex(
      theme_->findData(themeCode(currentTheme_)));

  selectors->addWidget(languageName, 0, 0);
  selectors->addWidget(themeName, 0, 1);
  selectors->addWidget(language_, 1, 0);
  selectors->addWidget(theme_, 1, 1);
  header->addLayout(selectors);

  statusBadge_ = new QLabel;
  statusBadge_->setObjectName("StatusBadge");
  statusBadge_->setProperty("controlId", "statusBadge");
  statusBadge_->setAlignment(Qt::AlignCenter);
  header->addWidget(statusBadge_);
  page->addLayout(header);

  auto* stateCard = new QFrame;
  stateCard->setObjectName("Card");
  auto* stateLayout = new QVBoxLayout(stateCard);
  auto* stateHeader = new QHBoxLayout;
  currentStateTitle_ = new QLabel;
  currentStateTitle_->setObjectName("CardTitle");
  refresh_ = new QPushButton;
  refresh_->setProperty("controlId", "refresh");
  refresh_->setObjectName("SecondaryButton");
  stateHeader->addWidget(currentStateTitle_);
  stateHeader->addStretch();
  stateHeader->addWidget(refresh_);
  stateLayout->addLayout(stateHeader);

  summary_ = new QLabel;
  summary_->setWordWrap(true);
  stateLayout->addWidget(summary_);

  slotsHost_ = new QWidget;
  slotsHost_->setObjectName("StatusSlots");
  slotsLayout_ = new QHBoxLayout(slotsHost_);
  slotsLayout_->setContentsMargins(0, 0, 0, 0);
  slotsLayout_->setSpacing(10);
  slotsHost_->setVisible(false);
  stateLayout->addWidget(slotsHost_);
  page->addWidget(stateCard);

  auto* runCard = new QFrame;
  runCard->setObjectName("Card");
  auto* runLayout = new QVBoxLayout(runCard);
  newExecutionTitle_ = new QLabel;
  newExecutionTitle_->setObjectName("CardTitle");
  runLayout->addWidget(newExecutionTitle_);

  auto* form = new QGridLayout;
  form->setHorizontalSpacing(14);
  form->setVerticalSpacing(10);

  puzzle_ = new QSpinBox;
  puzzle_->setProperty("controlId", "puzzle");
  puzzle_->setRange(1, 256);
  puzzle_->setValue(71);

  kangarooPuzzle_ = new QComboBox;
  kangarooPuzzle_->setProperty(
      "controlId", "kangarooPuzzle");
  for (const int value : {140, 145, 150, 155, 160}) {
    kangarooPuzzle_->addItem(
        QString::number(value),
        value);
  }

  mode_ = new QComboBox;
  mode_->setProperty("controlId", "executionMode");
  mode_->addItem(QString(), static_cast<int>(RunMode::BitCrackCuda));
  mode_->addItem(QString(), static_cast<int>(RunMode::BitCrackOpencl));
  mode_->addItem(QString(), static_cast<int>(RunMode::BitCrackCudaOpencl));
  mode_->addItem(QString(), static_cast<int>(RunMode::KeyHuntCpu));
  mode_->addItem(QString(), static_cast<int>(RunMode::KangarooCuda));

  device_ = new QSpinBox;
  device_->setProperty("controlId", "cudaDevice");
  device_->setRange(0, 31);
  device_->setValue(0);

  openclDevice_ = new QSpinBox;
  openclDevice_->setProperty(
      "controlId", "openclDevice");
  openclDevice_->setRange(0, 31);
  openclDevice_->setValue(1);

  cpuThreads_ = new QSpinBox;
  cpuThreads_->setProperty("controlId", "cpuThreads");
  const int logicalCpuCount =
      qMax(1, QThread::idealThreadCount());
  cpuThreads_->setRange(1, logicalCpuCount);
  cpuThreads_->setValue(qMax(1, logicalCpuCount - 1));

  rusticlRadeonsi_ = new QCheckBox;
  rusticlRadeonsi_->setProperty(
      "controlId", "rusticlRadeonsi");

  autoStart_ = new QCheckBox;
  autoStart_->setProperty("controlId", "autoStart");

  puzzleName_ = formLabel(QString());
  modeName_ = formLabel(QString());
  deviceName_ = formLabel(QString());
  openclDeviceName_ = formLabel(QString());
  cpuThreadsName_ = formLabel(QString());

  form->addWidget(puzzleName_, 0, 0);
  form->addWidget(puzzle_, 1, 0);
  form->addWidget(kangarooPuzzle_, 1, 0);
  form->addWidget(modeName_, 0, 1);
  form->addWidget(mode_, 1, 1, 1, 2);
  form->addWidget(deviceName_, 0, 3);
  form->addWidget(device_, 1, 3);
  form->addWidget(openclDeviceName_, 2, 0);
  form->addWidget(openclDevice_, 3, 0);
  form->addWidget(cpuThreadsName_, 2, 0);
  form->addWidget(cpuThreads_, 3, 0);
  form->addWidget(rusticlRadeonsi_, 3, 1, 1, 3);
  runLayout->addLayout(form);
  runLayout->addWidget(autoStart_);

  auto* actions = new QHBoxLayout;
  start_ = new QPushButton;
  start_->setProperty("controlId", "start");
  start_->setObjectName("PrimaryButton");
  safeStop_ = new QPushButton;
  safeStop_->setProperty("controlId", "safeStop");
  safeStop_->setObjectName("SecondaryButton");
  stop_ = new QPushButton;
  stop_->setProperty("controlId", "stop");
  stop_->setObjectName("DangerButton");
  actions->addWidget(start_);
  actions->addStretch();
  actions->addWidget(safeStop_);
  actions->addWidget(stop_);
  runLayout->addLayout(actions);
  page->addWidget(runCard);

  auto* toolsCard = new QFrame;
  toolsCard->setObjectName("Card");
  auto* toolsLayout = new QVBoxLayout(toolsCard);
  toolsTitle_ = new QLabel;
  toolsTitle_->setObjectName("CardTitle");
  toolsLayout->addWidget(toolsTitle_);

  auto* toolGrid = new QGridLayout;
  toolGrid->setSpacing(9);
  benchmark_ = new QPushButton;
  benchmark_->setProperty("controlId", "benchmark");
  selfTest_ = new QPushButton;
  selfTest_->setProperty("controlId", "selfTest");
  doctor_ = new QPushButton;
  doctor_->setProperty("controlId", "doctor");
  updates_ = new QPushButton;
  updates_->setProperty("controlId", "updates");
  audit_ = new QPushButton;
  audit_->setProperty("controlId", "audit");
  installKangaroo_ = new QPushButton;
  installKangaroo_->setProperty(
      "controlId", "installKangaroo");

  for (auto* button : {
           benchmark_,
           selfTest_,
           doctor_,
           updates_,
           audit_,
           installKangaroo_,
       }) {
    button->setObjectName("ToolButton");
  }

  toolGrid->addWidget(benchmark_, 0, 0);
  toolGrid->addWidget(selfTest_, 0, 1);
  toolGrid->addWidget(doctor_, 0, 2);
  toolGrid->addWidget(updates_, 1, 0);
  toolGrid->addWidget(audit_, 1, 1);
  toolGrid->addWidget(installKangaroo_, 1, 2);
  toolsLayout->addLayout(toolGrid);
  page->addWidget(toolsCard);

  detailsTitle_ = new QLabel;
  detailsTitle_->setObjectName("CardTitle");
  page->addWidget(detailsTitle_);

  detailsTabs_ = new QTabWidget;
  detailsTabs_->setObjectName("DetailsTabs");

  output_ = new QPlainTextEdit;
  output_->setProperty("controlId", "messagesOutput");
  output_->setReadOnly(true);
  output_->setMinimumHeight(180);
  output_->setMaximumHeight(280);

  statusOutput_ = new QPlainTextEdit;
  statusOutput_->setProperty("controlId", "statusOutput");
  statusOutput_->setReadOnly(true);
  statusOutput_->setMinimumHeight(180);
  statusOutput_->setMaximumHeight(280);

  runtimeLogOutput_ = new QPlainTextEdit;
  runtimeLogOutput_->setProperty(
      "controlId", "runtimeLogOutput");
  runtimeLogOutput_->setReadOnly(true);
  runtimeLogOutput_->setMinimumHeight(180);
  runtimeLogOutput_->setMaximumHeight(280);

  detailsTabs_->addTab(output_, QString());
  detailsTabs_->addTab(statusOutput_, QString());
  detailsTabs_->addTab(runtimeLogOutput_, QString());
  page->addWidget(detailsTabs_);
  page->addStretch(1);

  auto* scroll = new QScrollArea(this);
  scroll->setObjectName("PageScrollArea");
  scroll->viewport()->setObjectName("PageViewport");
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(
      Qt::ScrollBarAlwaysOff);
  scroll->setWidget(central);
  setCentralWidget(scroll);

  connect(
      refresh_,
      &QPushButton::clicked,
      this,
      &MainWindow::refreshStatus);
  connect(
      start_,
      &QPushButton::clicked,
      this,
      &MainWindow::startExecution);
  connect(
      safeStop_,
      &QPushButton::clicked,
      this,
      [this]() {
        runCommand("safe_stop", {"safestop"});
      });
  connect(
      stop_,
      &QPushButton::clicked,
      this,
      &MainWindow::confirmStop);
  connect(
      benchmark_,
      &QPushButton::clicked,
      this,
      [this]() {
        runCommand("benchmark", {"benchmark"});
      });
  connect(
      selfTest_,
      &QPushButton::clicked,
      this,
      &MainWindow::runSelfTest);
  connect(
      doctor_,
      &QPushButton::clicked,
      this,
      [this]() {
        runCommand("doctor", {"doctor"});
      });
  connect(
      updates_,
      &QPushButton::clicked,
      this,
      [this]() {
        runCommand("updates", {"update", "--check"});
      });
  connect(
      audit_,
      &QPushButton::clicked,
      this,
      [this]() {
        runCommand("audit", {"audit", "--limit", "20"});
      });
  connect(
      installKangaroo_,
      &QPushButton::clicked,
      this,
      &MainWindow::confirmKangarooInstall);
  connect(
      mode_,
      &QComboBox::currentIndexChanged,
      this,
      [this]() {
        QSettings().setValue(
            "run/mode",
            mode_->currentData().toInt());
        updateSelectionRules();
      });
  connect(
      puzzle_,
      &QSpinBox::valueChanged,
      this,
      [](int value) {
        QSettings().setValue("run/puzzle", value);
      });
  connect(
      kangarooPuzzle_,
      &QComboBox::currentIndexChanged,
      this,
      [this]() {
        QSettings().setValue(
            "run/kangaroo_puzzle",
            kangarooPuzzle_->currentData().toInt());
      });
  connect(
      device_,
      &QSpinBox::valueChanged,
      this,
      [](int value) {
        QSettings().setValue("run/device", value);
      });
  connect(
      openclDevice_,
      &QSpinBox::valueChanged,
      this,
      [](int value) {
        QSettings().setValue("run/opencl_device", value);
      });
  connect(
      cpuThreads_,
      &QSpinBox::valueChanged,
      this,
      [](int value) {
        QSettings().setValue("run/cpu_threads", value);
      });
  connect(
      rusticlRadeonsi_,
      &QCheckBox::toggled,
      this,
      [](bool checked) {
        QSettings().setValue("run/rusticl_radeonsi", checked);
      });
  connect(
      autoStart_,
      &QCheckBox::toggled,
      this,
      &MainWindow::configureAutomaticStart);
  connect(
      language_,
      &QComboBox::currentIndexChanged,
      this,
      [this]() {
        currentLanguage_ = languageFromCode(
            language_->currentData().toString());
        QSettings().setValue(
            "ui/language",
            languageCode(currentLanguage_));
        applyLanguage();
      });
  connect(
      theme_,
      &QComboBox::currentIndexChanged,
      this,
      [this]() {
        currentTheme_ = themeFromCode(
            theme_->currentData().toString());
        QSettings().setValue(
            "ui/theme",
            themeCode(currentTheme_));
        applyTheme();
      });

  updateSelectionRules();
  updateActionAvailability();
}

QString MainWindow::t(const QString& key) const {
  return translated(currentLanguage_, key);
}

RunMode MainWindow::selectedRunMode() const {
  return static_cast<RunMode>(
      mode_->currentData().toInt());
}

void MainWindow::applyLanguage() {
  subtitle_->setText(t("subtitle"));
  currentStateTitle_->setText(t("current_state"));
  newExecutionTitle_->setText(t("new_execution"));
  toolsTitle_->setText(t("tools"));
  detailsTitle_->setText(t("details"));

  puzzleName_->setText(t("puzzle"));
  modeName_->setText(t("execution_mode"));
  openclDeviceName_->setText(t("opencl_device"));
  cpuThreadsName_->setText(t("cpu_threads"));

  mode_->setItemText(0, t("bitcrack_cuda"));
  mode_->setItemText(1, t("bitcrack_opencl"));
  mode_->setItemText(2, t("bitcrack_cuda_opencl"));
  mode_->setItemText(3, t("keyhunt_cpu"));
  mode_->setItemText(4, t("kangaroo_cuda"));
  rusticlRadeonsi_->setText(t("rusticl_radeonsi"));
  autoStart_->setText(t("auto_start"));

  refresh_->setText(t("refresh"));
  start_->setText(t("start"));
  safeStop_->setText(t("safe_stop"));
  stop_->setText(t("stop_now"));
  benchmark_->setText(t("benchmark"));
  selfTest_->setText(t("self_test"));
  doctor_->setText(t("doctor"));
  updates_->setText(t("updates"));
  audit_->setText(t("audit"));
  installKangaroo_->setText(t("install_kangaroo"));

  for (auto* label : findChildren<QLabel*>()) {
    const QString key =
        label->property("translationKey").toString();
    if (!key.isEmpty()) {
      label->setText(t(key));
    }
  }

  theme_->setItemText(0, t("light"));
  theme_->setItemText(1, t("dark"));
  output_->setPlaceholderText(t("details_hint"));
  statusOutput_->setPlaceholderText(t("status_hint"));
  runtimeLogOutput_->setPlaceholderText(
      t("runtime_log_hint"));
  detailsTabs_->setTabText(0, t("messages"));
  detailsTabs_->setTabText(1, t("status_details"));
  detailsTabs_->setTabText(2, t("runtime_log"));
  statusBar()->showMessage(t("footer"));

  updateStatusBadge();
  summary_->setText(
      solutionFound_
          ? t("solution_saved")
          : (active_ ? t("runtime_active") : t("no_execution")));

  safeStop_->setToolTip(
      kangarooActive_
          ? t("kangaroo_stop_hint")
          : t("safe_stop_hint"));

  rebuildSlotCards(statusOutput_->toPlainText());

  updateSelectionRules();
  updateActionAvailability();
}

void MainWindow::applyTheme() {
  setStyleSheet(themeStyleSheet(currentTheme_));
}

void MainWindow::updateActionAvailability() {
  const bool ready = !busy_ && cliAvailable_;
  const bool idle = !active_ && !solutionFound_ && ready;

  start_->setEnabled(idle);
  safeStop_->setEnabled(
      ready && active_ && !kangarooActive_);
  stop_->setEnabled(ready && active_);
  refresh_->setEnabled(!busy_);

  benchmark_->setEnabled(idle);
  selfTest_->setEnabled(idle);
  installKangaroo_->setEnabled(idle);
  doctor_->setEnabled(ready);
  updates_->setEnabled(ready);
  audit_->setEnabled(ready);

  mode_->setEnabled(idle);
  puzzle_->setEnabled(idle);
  kangarooPuzzle_->setEnabled(idle);
  device_->setEnabled(idle);
  openclDevice_->setEnabled(idle);
  cpuThreads_->setEnabled(idle);
  rusticlRadeonsi_->setEnabled(idle);

  const QString busyHint =
      active_ ? t("busy_active") : QString();
  benchmark_->setToolTip(busyHint);
  selfTest_->setToolTip(busyHint);
  installKangaroo_->setToolTip(busyHint);
}

void MainWindow::updateSelectionRules() {
  const RunMode mode = selectedRunMode();
  const bool kangaroo = mode == RunMode::KangarooCuda;
  const bool cpu = mode == RunMode::KeyHuntCpu;
  const bool openclOnly = mode == RunMode::BitCrackOpencl;
  const bool concurrent = mode == RunMode::BitCrackCudaOpencl;
  const bool usesOpencl = openclOnly || concurrent;

  puzzle_->setVisible(!kangaroo);
  kangarooPuzzle_->setVisible(kangaroo);

  deviceName_->setVisible(!cpu);
  device_->setVisible(!cpu);
  deviceName_->setText(
      openclOnly
          ? t("opencl_device")
          : t("cuda_device"));

  openclDeviceName_->setVisible(concurrent);
  openclDevice_->setVisible(concurrent);

  cpuThreadsName_->setVisible(cpu);
  cpuThreads_->setVisible(cpu);

  rusticlRadeonsi_->setVisible(usesOpencl);
}

QString MainWindow::cliExecutable() const {
  const QString overridePath =
      qEnvironmentVariable("OPENPUZZLE_CLI");

  if (!overridePath.isEmpty()) {
    return overridePath;
  }

  return QStandardPaths::findExecutable("openpuzzle");
}

void MainWindow::refreshStatus() {
  if (statusProcess_->state() != QProcess::NotRunning) {
    return;
  }

  const QString executable = cliExecutable();
  if (executable.isEmpty()) {
    updateStatusBadge();
    summary_->setText(t("cli_missing"));
    cliAvailable_ = false;
    updateActionAvailability();
    return;
  }

  cliAvailable_ = true;
  updateActionAvailability();
  statusProcess_->start(
      executable,
      QStringList{QStringLiteral("status")});
  statusTimeout_->start();
}

void MainWindow::handleStatusResult() {
  const QString result = QString::fromUtf8(
      statusProcess_->readAll()).trimmed();

  busy_ = false;

  if (result.isEmpty()) {
    updateStatusBadge();
    summary_->setText(t("empty_status"));
    updateActionAvailability();
    return;
  }

  setTextPreservingScroll(statusOutput_, result);
  refreshRuntimeLog();

  const auto runtimeSlots = parseRuntimeSlots(result);
  QString solutionAssignmentId;
  for (const auto& slot : runtimeSlots) {
    if (slot.fields.value("Status").compare(
            "solution found",
            Qt::CaseInsensitive) == 0) {
      solutionAssignmentId = slot.fields.value("Assignment");
      if (solutionAssignmentId.isEmpty()) {
        solutionAssignmentId = QStringLiteral("solution");
      }
      break;
    }
  }

  solutionFound_ = !solutionAssignmentId.isEmpty();
  active_ =
      result.contains("Status............. running") ||
      result.contains("Status............. waiting") ||
      result.contains("Runtime PID........");

  kangarooActive_ =
      active_ &&
      result.contains("Engine............. Kangaroo");

  updateStatusBadge();

  safeStop_->setToolTip(
      kangarooActive_
          ? t("kangaroo_stop_hint")
          : t("safe_stop_hint"));
  updateActionAvailability();

  if (solutionFound_) {
    summary_->setText(t("solution_saved"));
    rebuildSlotCards(result);
    showSolutionNotice(solutionAssignmentId);
    return;
  }

  if (!active_) {
    summary_->setText(t("no_execution"));
    rebuildSlotCards(result);
    return;
  }
  rebuildSlotCards(result);
  summary_->setText(t("runtime_active"));
}

void MainWindow::updateStatusBadge() {
  statusBadge_->setText(
      active_ ? t("running") : t("stopped"));
  statusBadge_->setProperty("active", active_);
  statusBadge_->style()->unpolish(statusBadge_);
  statusBadge_->style()->polish(statusBadge_);
}

void MainWindow::refreshRuntimeLog() {
  QFile file(runtimeLogPath());
  if (!file.open(QIODevice::ReadOnly)) {
    return;
  }

  constexpr qint64 maximumVisibleBytes =
      128 * 1024;
  const qint64 offset =
      qMax<qint64>(0, file.size() - maximumVisibleBytes);

  if (offset > 0) {
    file.seek(offset);
  }

  QByteArray contents = file.readAll();
  if (offset > 0) {
    const qsizetype firstNewline =
        contents.indexOf('\n');
    if (firstNewline >= 0) {
      contents.remove(0, firstNewline + 1);
    }
  }

  setTextPreservingScroll(
      runtimeLogOutput_,
      QString::fromUtf8(contents).trimmed());
}

void MainWindow::showSolutionNotice(
    const QString& assignmentId) {
  if (assignmentId == lastSolutionNoticeId_) {
    return;
  }

  lastSolutionNoticeId_ = assignmentId;
  showOutput(t("solution_found"), t("solution_saved"));
}

QString MainWindow::automaticStartUnitPath() const {
  return QStandardPaths::writableLocation(
      QStandardPaths::ConfigLocation) +
      "/systemd/user/openpuzzle-ui-autostart.service";
}

QString MainWindow::runtimeLogPath() const {
  const QString overridePath =
      qEnvironmentVariable(
          "OPENPUZZLE_UI_RUNTIME_LOG");
  if (!overridePath.isEmpty()) {
    return overridePath;
  }

  return QStandardPaths::writableLocation(
      QStandardPaths::GenericDataLocation) +
      "/OpenPuzzle/ui-runtime.log";
}

QString MainWindow::systemctlExecutable() const {
  const QString overridePath =
      qEnvironmentVariable("OPENPUZZLE_SYSTEMCTL");
  return overridePath.isEmpty()
      ? QStandardPaths::findExecutable("systemctl")
      : overridePath;
}

bool MainWindow::runSystemctl(
    const QStringList& arguments,
    QString& error) const {
  const QString executable = systemctlExecutable();
  if (executable.isEmpty()) {
    error = "systemctl was not found";
    return false;
  }

  QProcess process;
  process.setProcessChannelMode(QProcess::MergedChannels);
  process.start(executable, arguments);
  if (!process.waitForStarted(3000)) {
    error = process.errorString();
    return false;
  }
  if (!process.waitForFinished(10000)) {
    process.kill();
    process.waitForFinished(1000);
    error = "systemctl timed out";
    return false;
  }
  if (process.exitStatus() != QProcess::NormalExit ||
      process.exitCode() != 0) {
    error = QString::fromUtf8(process.readAll()).trimmed();
    return false;
  }
  return true;
}

bool MainWindow::writeAutomaticStartUnit(
    QString& error) {
  const QString executable = cliExecutable();
  if (executable.isEmpty()) {
    error = t("cli_missing");
    return false;
  }

  RunSelection selection;
  selection.mode = selectedRunMode();
  selection.puzzle =
      selection.mode == RunMode::KangarooCuda
          ? kangarooPuzzle_->currentData().toInt()
          : puzzle_->value();
  selection.device = device_->value();
  selection.openclDevice = openclDevice_->value();
  selection.cpuThreads = cpuThreads_->value();
  selection.rusticlRadeonsi = rusticlRadeonsi_->isChecked();

  QStringList arguments;
  try {
    for (const auto& argument :
         RunCommandBuilder::build(selection)) {
      arguments.push_back(QString::fromStdString(argument));
    }
  } catch (const std::exception& exception) {
    error = QString::fromUtf8(exception.what());
    return false;
  }

  const QString unitPath = automaticStartUnitPath();
  if (!QDir().mkpath(QFileInfo(unitPath).absolutePath())) {
    error = "Unable to create the systemd user directory";
    return false;
  }

  QStringList quotedCommand{systemdQuote(executable)};
  for (const QString& argument : arguments) {
    quotedCommand.push_back(systemdQuote(argument));
  }

  QSaveFile unit(unitPath);
  if (!unit.open(QIODevice::WriteOnly | QIODevice::Text)) {
    error = unit.errorString();
    return false;
  }
  if (!unit.setPermissions(
          QFileDevice::ReadOwner |
          QFileDevice::WriteOwner)) {
    error = unit.errorString();
    return false;
  }

  const QString contents =
      "[Unit]\n"
      "Description=OpenPuzzle automatic search\n"
      "Wants=network-online.target\n"
      "After=network-online.target\n\n"
      "[Service]\n"
      "Type=simple\n"
      "ExecStart=" + quotedCommand.join(' ') + "\n"
      "Restart=on-failure\n"
      "RestartSec=30s\n"
      "TimeoutStopSec=180s\n"
      "KillMode=mixed\n"
      "KillSignal=SIGTERM\n"
      "UMask=0077\n"
      "WorkingDirectory=%h\n"
      "StandardOutput=journal\n"
      "StandardError=journal\n\n"
      "[Install]\n"
      "WantedBy=default.target\n";

  const QByteArray unitBytes = contents.toUtf8();
  if (unit.write(unitBytes) != unitBytes.size() ||
      !unit.commit()) {
    error = unit.errorString();
    return false;
  }
  return true;
}

void MainWindow::configureAutomaticStart(bool enabled) {
  autoStart_->setEnabled(false);
  QString error;
  bool success = true;

  if (enabled) {
    success = writeAutomaticStartUnit(error) &&
        runSystemctl({"--user", "daemon-reload"}, error) &&
        runSystemctl(
            {"--user", "enable", "openpuzzle-ui-autostart.service"},
            error);
  } else {
    success = runSystemctl(
        {"--user", "disable", "openpuzzle-ui-autostart.service"},
        error);
    if (success && QFile::exists(automaticStartUnitPath())) {
      success = QFile::remove(automaticStartUnitPath());
      if (!success) {
        error = "Unable to remove the systemd user service";
      }
    }
    if (success) {
      success = runSystemctl({"--user", "daemon-reload"}, error);
    }
  }

  if (!success) {
    const QSignalBlocker blocker(autoStart_);
    autoStart_->setChecked(!enabled);
    showOutput(t("auto_start_failed"), error);
  } else {
    QSettings().setValue("run/auto_start", enabled);
    showOutput(
        t(enabled ? "auto_start_enabled" : "auto_start_disabled"),
        t(enabled
              ? "auto_start_enabled_message"
              : "auto_start_disabled_message"));
  }
  autoStart_->setEnabled(true);
}

void MainWindow::rebuildSlotCards(
    const QString& statusOutput) {
  while (auto* item = slotsLayout_->takeAt(0)) {
    if (auto* widget = item->widget()) {
      widget->deleteLater();
    }
    delete item;
  }

  if (!active_) {
    slotsHost_->setVisible(false);
    return;
  }

  const auto runtimeSlots = parseRuntimeSlots(statusOutput);
  for (const auto& slot : runtimeSlots) {
    const QString state = slot.fields.value("Status").toLower();
    if (state != "running" &&
        state != "waiting" &&
        !slot.fields.contains("Runtime PID")) {
      continue;
    }

    auto* card = new QFrame;
    card->setObjectName("SlotCard");
    card->setProperty("controlId", "runtimeSlot");

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(12, 10, 12, 10);
    cardLayout->setSpacing(8);

    QString slotName = slot.fields.value("Slot").toUpper();
    if (slotName.isEmpty()) {
      slotName = slot.fields.value("Backend").toUpper();
    }
    if (slotName.isEmpty()) {
      slotName = t("primary_slot");
    }

    auto* title = new QLabel(
        t("slot") + " " + slotName);
    title->setObjectName("SlotTitle");
    cardLayout->addWidget(title);

    auto* metrics = new QGridLayout;
    metrics->setContentsMargins(0, 0, 0, 0);
    metrics->setHorizontalSpacing(8);
    metrics->setVerticalSpacing(8);

    QString assignment = slot.fields.value("Assignment number");
    if (!assignment.isEmpty()) {
      assignment.prepend('#');
    } else {
      assignment = statusField(slot, "Assignment");
    }

    metrics->addWidget(
        createMetric(t("puzzle"), statusField(slot, "Puzzle")),
        0, 0);
    metrics->addWidget(
        createMetric(t("assignment"), assignment),
        0, 1);
    metrics->addWidget(
        createMetric(t("engine"), statusField(slot, "Engine")),
        1, 0);
    metrics->addWidget(
        createMetric(t("backend"), statusField(slot, "Backend")),
        1, 1);
    metrics->addWidget(
        createMetric(t("speed"), statusField(slot, "Speed")),
        2, 0);
    metrics->addWidget(
        createMetric(t("progress"), statusField(slot, "Progress")),
        2, 1);

    cardLayout->addLayout(metrics);
    slotsLayout_->addWidget(card, 1);
  }

  slotsHost_->setVisible(slotsLayout_->count() > 0);
}

void MainWindow::startExecution() {
  const QString executable = cliExecutable();
  if (executable.isEmpty()) {
    QMessageBox::critical(
        this,
        "OpenPuzzle",
        t("cli_missing"));
    return;
  }

  RunSelection selection;
  selection.mode = selectedRunMode();
  selection.puzzle =
      selection.mode == RunMode::KangarooCuda
          ? kangarooPuzzle_->currentData().toInt()
          : puzzle_->value();
  selection.device = device_->value();
  selection.openclDevice = openclDevice_->value();
  selection.cpuThreads = cpuThreads_->value();
  selection.rusticlRadeonsi =
      rusticlRadeonsi_->isChecked();

  QStringList arguments;

  try {
    for (const auto& argument :
         RunCommandBuilder::build(selection)) {
      arguments.push_back(
          QString::fromStdString(argument));
    }
  } catch (const std::exception& error) {
    QMessageBox::warning(
        this,
        t("invalid_configuration"),
        QString::fromUtf8(error.what()));
    return;
  }

  const QString logPath = runtimeLogPath();
  if (!QDir().mkpath(
          QFileInfo(logPath).absolutePath())) {
    QMessageBox::critical(
        this,
        "OpenPuzzle",
        t("runtime_log_failed"));
    return;
  }

  QFile logFile(logPath);
  if (!logFile.open(
          QIODevice::WriteOnly |
          QIODevice::Truncate) ||
      !logFile.setPermissions(
          QFileDevice::ReadOwner |
          QFileDevice::WriteOwner)) {
    QMessageBox::critical(
        this,
        "OpenPuzzle",
        t("runtime_log_failed"));
    return;
  }
  logFile.close();

  QProcess launcher;
  launcher.setProgram(executable);
  launcher.setArguments(arguments);
  launcher.setProcessChannelMode(
      QProcess::MergedChannels);
  launcher.setStandardInputFile(
      QProcess::nullDevice());
  launcher.setStandardOutputFile(
      logPath,
      QIODevice::Append);

  qint64 processId = 0;
  if (!launcher.startDetached(&processId)) {
    QMessageBox::critical(
        this,
        "OpenPuzzle",
        t("start_failed"));
    return;
  }

  showOutput(
      t("execution_started"),
      executable + " " + arguments.join(' ') +
          "\n" + t("launcher_pid") + ": " +
          QString::number(processId) +
          "\n" + t("runtime_log_path") + ": " +
          logPath);

  busy_ = true;
  updateActionAvailability();
  QTimer::singleShot(
      1200,
      this,
      &MainWindow::refreshStatus);
}

void MainWindow::runCommand(
    const QString& titleKey,
    const QStringList& arguments) {
  const QString executable = cliExecutable();
  if (executable.isEmpty()) {
    QMessageBox::critical(
        this,
        "OpenPuzzle",
        t("cli_missing"));
    return;
  }

  setBusy(true);
  auto* process = new QProcess(this);
  process->setProcessChannelMode(
      QProcess::MergedChannels);

  connect(
      process,
      &QProcess::finished,
      this,
      [this, process, titleKey](
          int,
          QProcess::ExitStatus) {
        const QString result = QString::fromUtf8(
            process->readAll()).trimmed();
        showOutput(
            t(titleKey),
            result);
        process->deleteLater();
        setBusy(false);
        QTimer::singleShot(
            700,
            this,
            &MainWindow::refreshStatus);
      });

  connect(
      process,
      &QProcess::errorOccurred,
      this,
      [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) {
          return;
        }

        showOutput(
            t("error"),
            t("control_failed"));
        process->deleteLater();
        setBusy(false);
      });

  process->start(executable, arguments);
}

void MainWindow::runSelfTest() {
  QString backend;
  int device = device_->value();

  switch (selectedRunMode()) {
  case RunMode::BitCrackOpencl:
    backend = "opencl";
    break;
  case RunMode::KeyHuntCpu:
    backend = "cpu";
    break;
  case RunMode::KangarooCuda:
    backend = "kangaroo";
    break;
  case RunMode::BitCrackCudaOpencl:
  case RunMode::BitCrackCuda:
  default:
    backend = "cuda";
    break;
  }

  QStringList arguments = {
      "selftest",
      "--backend",
      backend,
  };

  if (backend != "cpu") {
    arguments << "--device" << QString::number(device);
  }

  if (
      backend == "opencl" &&
      rusticlRadeonsi_->isChecked()) {
    arguments << "--rusticl-enable" << "radeonsi";
  }

  runCommand("self_test", arguments);
}

void MainWindow::confirmStop() {
  if (QMessageBox::question(
          this,
          t("stop_title"),
          t("stop_question"),
          QMessageBox::Yes | QMessageBox::No,
          QMessageBox::No) == QMessageBox::Yes) {
    runCommand("stop_now", {"stop"});
  }
}

void MainWindow::confirmKangarooInstall() {
  if (QMessageBox::question(
          this,
          t("install_title"),
          t("install_question"),
          QMessageBox::Yes | QMessageBox::No,
          QMessageBox::No) == QMessageBox::Yes) {
    runCommand(
        "install_kangaroo",
        {"engine", "install", "psckangaroo"});
  }
}

void MainWindow::setBusy(bool busy) {
  busy_ = busy;
  updateActionAvailability();
}

void MainWindow::showOutput(
    const QString& title,
    const QString& output) {
  output_->setPlainText(
      title + "\n" +
      QString(title.size(), '=') + "\n" +
      (
          output.isEmpty()
              ? t("no_message")
              : output
      ));
  detailsTabs_->setCurrentIndex(0);
  statusBar()->showMessage(title, 5000);
}

} // namespace openpuzzle::ui

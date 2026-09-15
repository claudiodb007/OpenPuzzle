#include "UiTheme.hpp"

namespace openpuzzle::ui {

QString themeCode(UiTheme theme) {
  return theme == UiTheme::Dark ? "dark" : "light";
}

UiTheme themeFromCode(const QString& code) {
  return code == "dark"
      ? UiTheme::Dark
      : UiTheme::Light;
}

QString themeStyleSheet(UiTheme theme) {
  const bool dark = theme == UiTheme::Dark;

  const QString background = dark ? "#080b10" : "#f4f6fa";
  const QString panel = dark ? "#10151d" : "#ffffff";
  const QString panelSoft = dark ? "#171d27" : "#f7f8fc";
  const QString border = dark ? "#29313d" : "#e0e5ed";
  const QString text = dark ? "#f5f1e7" : "#182235";
  const QString muted = dark ? "#a6adba" : "#68738a";
  const QString input = dark ? "#0c1118" : "#ffffff";
  const QString accent = dark ? "#e3b23c" : "#bd8616";
  const QString accentHover = dark ? "#f0c45b" : "#a97410";
  const QString accentText = dark ? "#111318" : "#ffffff";
  const QString secondary = dark ? "#222a36" : "#e9edf5";
  const QString danger = "#b91c1c";
  const QString dangerText = "#ffffff";

  return QString(R"(
    QMainWindow {
      background: %1;
      color: %2;
      font-size: 15px;
    }
    QWidget {
      background: transparent;
      color: %2;
      font-size: 15px;
    }
    QWidget#PageBackground,
    QWidget#PageContent,
    QWidget#PageViewport,
    QScrollArea#PageScrollArea {
      background: %1;
    }
    QLabel#LogoPlate {
      background: #080a0d;
      border: 1px solid %3;
      border-radius: 13px;
      padding: 3px;
    }
    QLabel#Title {
      font-size: 30px;
      font-weight: 700;
      color: %2;
    }
    QLabel#Subtitle, QLabel#FormLabel {
      color: %4;
    }
    QLabel#Subtitle { font-size: 13px; }
    QLabel#FormLabel {
      font-size: 12px;
      font-weight: 600;
    }
    QLabel#CardTitle {
      font-size: 17px;
      font-weight: 650;
    }
    QLabel#SlotTitle {
      color: %11;
      font-size: 15px;
      font-weight: 700;
    }
    QLabel#StatusBadge {
      background: %14;
      border-radius: 13px;
      color: %15;
      font-weight: 650;
      min-width: 96px;
      padding: 6px 12px;
    }
    QLabel#StatusBadge[active="true"] {
      background: %6;
      color: %7;
    }
    QFrame#Card {
      background: %8;
      border: 1px solid %3;
      border-radius: 12px;
      padding: 12px;
    }
    QFrame#Metric {
      background: %9;
      border: 1px solid %3;
      border-radius: 8px;
    }
    QFrame#SlotCard {
      background: %8;
      border: 1px solid %3;
      border-radius: 10px;
    }
    QLabel#MetricName {
      color: %4;
      font-size: 11px;
      font-weight: 650;
    }
    QLabel#MetricValue {
      color: %2;
      font-size: 14px;
      font-weight: 700;
    }
    QComboBox, QSpinBox, QLineEdit, QPlainTextEdit {
      background: %10;
      color: %2;
      border: 1px solid %3;
      border-radius: 7px;
      padding: 8px;
      selection-background-color: %11;
    }
    QComboBox QAbstractItemView {
      background: %8;
      color: %2;
      selection-background-color: %11;
    }
    QComboBox:focus, QSpinBox:focus, QLineEdit:focus {
      border: 1px solid %11;
    }
    QPushButton {
      border: 0;
      border-radius: 8px;
      font-weight: 650;
      padding: 9px 16px;
    }
    QPushButton#PrimaryButton {
      background: %11;
      color: %12;
    }
    QPushButton#PrimaryButton:hover { background: %13; }
    QPushButton#SecondaryButton {
      background: %5;
      color: %2;
    }
    QPushButton#DangerButton {
      background: %14;
      color: %15;
    }
    QPushButton#ToolButton {
      background: %9;
      color: %2;
      border: 1px solid %3;
      text-align: left;
      padding: 11px 14px;
    }
    QPushButton#ToolButton:hover {
      border: 1px solid %11;
      color: %11;
    }
    QPushButton#PrimaryButton:disabled,
    QPushButton#SecondaryButton:disabled,
    QPushButton#DangerButton:disabled,
    QPushButton#ToolButton:disabled {
      background: %5;
      color: %4;
      border-color: %3;
    }
    QPlainTextEdit {
      font-family: monospace;
    }
    QTabWidget#DetailsTabs::pane {
      background: %8;
      border: 1px solid %3;
      border-radius: 8px;
      top: -1px;
    }
    QTabBar::tab {
      background: %9;
      color: %4;
      border: 1px solid %3;
      padding: 8px 16px;
    }
    QTabBar::tab:selected {
      background: %8;
      color: %11;
      border-bottom-color: %8;
    }
    QScrollBar:vertical {
      background: %8;
      border: 0;
      width: 12px;
      margin: 0;
    }
    QScrollBar::handle:vertical {
      background: %5;
      border-radius: 5px;
      min-height: 30px;
    }
    QScrollBar::handle:vertical:hover {
      background: %11;
    }
    QScrollBar::add-line:vertical,
    QScrollBar::sub-line:vertical,
    QScrollBar::add-page:vertical,
    QScrollBar::sub-page:vertical {
      background: transparent;
      border: 0;
      height: 0;
    }
    QStatusBar {
      background: %8;
      color: %4;
    }
  )")
      .arg(background)
      .arg(text)
      .arg(border)
      .arg(muted)
      .arg(secondary)
      .arg("#15803d")
      .arg("#ffffff")
      .arg(panel)
      .arg(panelSoft)
      .arg(input)
      .arg(accent)
      .arg(accentText)
      .arg(accentHover)
      .arg(danger)
      .arg(dangerText);
}

} // namespace openpuzzle::ui

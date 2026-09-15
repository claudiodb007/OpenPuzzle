#pragma once

#include <QString>

namespace openpuzzle::ui {

enum class UiTheme {
  Light,
  Dark,
};

QString themeCode(UiTheme theme);
UiTheme themeFromCode(const QString& code);
QString themeStyleSheet(UiTheme theme);

} // namespace openpuzzle::ui

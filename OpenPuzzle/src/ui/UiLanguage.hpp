#pragma once

#include <QString>

namespace openpuzzle::ui {

enum class UiLanguage {
  English = 0,
  Portuguese = 1,
  French = 2,
  Spanish = 3,
};

QString languageCode(UiLanguage language);
UiLanguage languageFromCode(const QString& code);
QString translated(UiLanguage language, const QString& key);

} // namespace openpuzzle::ui

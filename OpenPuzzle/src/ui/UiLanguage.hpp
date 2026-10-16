#pragma once
#include "openpuzzle/ui/UiTranslations.hpp"
#include <QString>
namespace openpuzzle::ui {
QString languageCode(UiLanguage language);
UiLanguage languageFromCode(const QString& code);
QString translated(UiLanguage language,const QString& key);
}

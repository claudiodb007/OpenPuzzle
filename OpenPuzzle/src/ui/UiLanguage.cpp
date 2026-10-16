#include "UiLanguage.hpp"
namespace openpuzzle::ui {
QString languageCode(UiLanguage language){const auto text=uiLanguageCode(language);return QString::fromUtf8(text.data(),static_cast<int>(text.size()));}
UiLanguage languageFromCode(const QString& code){const auto utf8=code.toUtf8();return uiLanguageFromCode({utf8.constData(),static_cast<std::size_t>(utf8.size())});}
QString translated(UiLanguage language,const QString& key){const auto utf8=key.toUtf8();const auto text=uiText(language,{utf8.constData(),static_cast<std::size_t>(utf8.size())});return QString::fromUtf8(text.data(),static_cast<int>(text.size()));}
}

#pragma once
#ifdef _WIN32
#include "openpuzzle/ui/UiTranslations.hpp"
#include <windows.h>
#include <utility>
namespace openpuzzle::ui {
// UI preference only: opening the panel never creates identity or runtime data.
class WindowsUiLanguageStore {
 std::wstring key_;
public:
 explicit WindowsUiLanguageStore(std::wstring key=L"Software\\OpenPuzzle\\WindowsPanel"):key_(std::move(key)){}
 UiLanguage load() const {
  wchar_t value[16]{};DWORD bytes=sizeof(value);
  if(RegGetValueW(HKEY_CURRENT_USER,key_.c_str(),L"Language",RRF_RT_REG_SZ|RRF_ZEROONFAILURE,nullptr,value,&bytes)!=ERROR_SUCCESS)return UiLanguage::English;
  if(bytes<sizeof(wchar_t)||bytes>sizeof(value)||bytes%sizeof(wchar_t)||value[bytes/sizeof(wchar_t)-1]!=L'\0')return UiLanguage::English;
  // Reject embedded NULs and unknown codes instead of partially accepting data.
  const std::wstring code(value,bytes/sizeof(wchar_t)-1);
  for(unsigned i=0;i<uiLanguageCodes.size();++i){const auto c=uiLanguageCodes[i];if(code==std::wstring(c.begin(),c.end()))return static_cast<UiLanguage>(i);}
  return UiLanguage::English;
 }
 bool save(UiLanguage language) const {
  if(static_cast<unsigned>(language)>=uiLanguageCodes.size())return false;
  HKEY key=nullptr;
  if(RegCreateKeyExW(HKEY_CURRENT_USER,key_.c_str(),0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr)!=ERROR_SUCCESS)return false;
  const auto code=uiLanguageCode(language);const std::wstring value(code.begin(),code.end());
  const auto status=RegSetValueExW(key,L"Language",0,REG_SZ,reinterpret_cast<const BYTE*>(value.c_str()),static_cast<DWORD>((value.size()+1)*sizeof(wchar_t)));
  RegCloseKey(key);return status==ERROR_SUCCESS;
 }
};
}
#endif

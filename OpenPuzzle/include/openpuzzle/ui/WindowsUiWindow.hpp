#pragma once
#ifdef _WIN32
#include <filesystem>
#include <string>
namespace openpuzzle::ui {
int runWindowsUi(const std::filesystem::path& cli,bool smokeTest=false,
 const std::wstring& languageKey=L"Software\\OpenPuzzle\\WindowsPanel",const std::filesystem::path& fixtureProfile={});
}
#endif

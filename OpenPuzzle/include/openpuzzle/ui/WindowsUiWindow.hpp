#pragma once
#ifdef _WIN32
#include <filesystem>
namespace openpuzzle::ui {
int runWindowsUi(const std::filesystem::path& cli,bool smokeTest=false);
}
#endif

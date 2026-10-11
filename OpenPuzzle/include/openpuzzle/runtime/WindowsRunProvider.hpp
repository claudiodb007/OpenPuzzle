#pragma once
#ifdef _WIN32
#include "openpuzzle/runtime/WindowsRunSession.hpp"
namespace openpuzzle {
struct WindowsPreparedRun {RunCommandOptions options;WindowsRunDependencies dependencies;};
WindowsPreparedRun prepareWindowsRun(RunCommandOptions options,const std::filesystem::path& engineFolder);
}
#endif

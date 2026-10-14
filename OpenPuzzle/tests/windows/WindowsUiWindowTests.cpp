#include "openpuzzle/ui/WindowsUiWindow.hpp"
#include <iostream>
int wmain(int argc,wchar_t** argv){if(argc!=2)return 1;const auto result=openpuzzle::ui::runWindowsUi(std::filesystem::absolute(argv[1]),true);if(!result)std::cout<<"WindowsUiWindowTests passed: native controls, asynchronous local readings and blocked close during runtime\n";return result;}

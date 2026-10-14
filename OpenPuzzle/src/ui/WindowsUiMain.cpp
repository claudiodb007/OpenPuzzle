#include "openpuzzle/ui/WindowsUiWindow.hpp"
#include "openpuzzle/ui/WindowsUiProcess.hpp"
#include <array>
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
 std::array<wchar_t,32768> executable{};const auto count=GetModuleFileNameW(nullptr,executable.data(),static_cast<DWORD>(executable.size()));
 if(!count||count>=executable.size())return 1;
 return openpuzzle::ui::runWindowsUi(std::filesystem::path(executable.data()).parent_path()/L"openpuzzle.exe");
}

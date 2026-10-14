#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <filesystem>
#include <string>
#include <vector>
namespace openpuzzle::ui {
struct UiProcessUpdate {std::string output;bool finished=false,timedOut=false;int exitCode=-1;};
// Pumped on the window timer: no waits on the UI thread, no shell, no parent
// environment changes. Only short owned commands may be terminated on timeout.
class WindowsUiProcess {
 HANDLE process_=nullptr,pipe_=nullptr;
 bool runtime_=false;ULONGLONG deadline_=0;
 void release();
public:
 ~WindowsUiProcess();
 WindowsUiProcess()=default;
 WindowsUiProcess(const WindowsUiProcess&)=delete;
 WindowsUiProcess& operator=(const WindowsUiProcess&)=delete;
 bool busy()const{return process_!=nullptr;}
 bool runtime()const{return busy()&&runtime_;}
 void start(const std::filesystem::path& cli,const std::vector<std::string>& args,bool runtime=false,DWORD timeoutMs=30000);
 UiProcessUpdate poll();
};
}
#endif

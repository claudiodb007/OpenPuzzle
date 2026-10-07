#include "openpuzzle/runtime/WindowsProcessIdentity.hpp"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace openpuzzle;
void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
int wmain(int argc, wchar_t **argv) {
  PROCESS_INFORMATION child{};
  try {
    require(argc == 2, "fixture path required");
    require(!WindowsProcessIdentity::creationTime(0), "PID zero rejected");
    require(!WindowsProcessIdentity::terminateIfMatches(0, 1), "invalid termination rejected");
    auto current = WindowsProcessIdentity::creationTime(GetCurrentProcessId());
    require(current && *current != 0, "current process creation time available");
    require(WindowsProcessIdentity::creationTime(GetCurrentProcessId()) == current, "stable creation time");
    auto executable = std::filesystem::absolute(argv[1]).wstring();
    auto command = L"\"" + executable + L"\" sleep";
    STARTUPINFOW startup{}; startup.cb = sizeof(startup);
    require(CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE,
                           CREATE_NO_WINDOW, nullptr, nullptr, &startup, &child), "child created");
    auto identity = WindowsProcessIdentity::creationTime(child.dwProcessId);
    require(identity && *identity != 0, "child identity available");
    require(!WindowsProcessIdentity::terminateIfMatches(child.dwProcessId, *identity + 1), "mismatched identity rejected");
    require(WaitForSingleObject(child.hProcess, 0) == WAIT_TIMEOUT, "mismatched identity preserves child");
    require(!WindowsProcessIdentity::terminateIfMatches(child.dwProcessId, 0), "missing expected identity rejected");
    require(WindowsProcessIdentity::terminateIfMatches(child.dwProcessId, *identity, 77), "matching identity permits termination");
    require(WaitForSingleObject(child.hProcess, 5000) == WAIT_OBJECT_0, "child exited");
    DWORD code = 0; require(GetExitCodeProcess(child.hProcess, &code) && code == 77, "termination code preserved");
    require(!WindowsProcessIdentity::terminateIfMatches(child.dwProcessId, *identity), "exited process not terminated again");
    CloseHandle(child.hThread); CloseHandle(child.hProcess);
    std::cout << "WindowsProcessIdentityTests passed\n"; return 0;
  } catch (const std::exception &error) {
    if (child.hProcess) { TerminateProcess(child.hProcess, 125); WaitForSingleObject(child.hProcess, 5000); CloseHandle(child.hProcess); }
    if (child.hThread) CloseHandle(child.hThread);
    std::cerr << error.what() << '\n'; return 1;
  }
}

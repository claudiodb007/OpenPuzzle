#include "openpuzzle/runtime/WindowsProcessIdentity.hpp"
#include "openpuzzle/core/WindowsPrivateFile.hpp"
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
    require(WindowsProcessIdentity::status(0, 1) == WindowsProcessIdentity::Status::Unavailable, "invalid PID cannot authorize cleanup");
    require(WindowsProcessIdentity::status(GetCurrentProcessId(), 0) == WindowsProcessIdentity::Status::Unavailable, "missing identity unavailable");
    require(WindowsProcessIdentity::status(GetCurrentProcessId(), *current) == WindowsProcessIdentity::Status::Running, "matching process running");
    require(WindowsProcessIdentity::status(GetCurrentProcessId(), *current + 1) == WindowsProcessIdentity::Status::Inactive, "mismatched creation time inactive");
    auto executable = std::filesystem::absolute(argv[1]).wstring();
    auto command = L"\"" + executable + L"\" sleep";
    STARTUPINFOW startup{}; startup.cb = sizeof(startup);
    require(CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE,
                           CREATE_NO_WINDOW, nullptr, nullptr, &startup, &child), "child created");
    auto identity = WindowsProcessIdentity::creationTime(child.dwProcessId);
    require(identity && *identity != 0, "child identity available");
    // Change only the owned fixture's process DACL. Existing creation handles
    // remain usable for restoration/cleanup, while new identity opens fail.
    PACL originalAcl = nullptr;
    PSECURITY_DESCRIPTOR originalDescriptor = nullptr;
    require(GetSecurityInfo(child.hProcess, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION,
        nullptr, nullptr, &originalAcl, nullptr, &originalDescriptor) == ERROR_SUCCESS, "fixture process ACL readable");
    windowsFile::Security user;
    EXPLICIT_ACCESSW entries[2]{};
    for (auto& entry : entries) {
      entry.Trustee.TrusteeForm = TRUSTEE_IS_SID;
      entry.Trustee.TrusteeType = TRUSTEE_IS_USER;
      entry.Trustee.ptstrName = reinterpret_cast<LPWSTR>(user.sid());
    }
    entries[0].grfAccessMode = DENY_ACCESS;
    entries[0].grfAccessPermissions = PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE;
    entries[1].grfAccessMode = SET_ACCESS;
    entries[1].grfAccessPermissions = PROCESS_ALL_ACCESS;
    PACL deniedAcl = nullptr;
    require(SetEntriesInAclW(2, entries, nullptr, &deniedAcl) == ERROR_SUCCESS, "fixture process denial ACL created");
    const auto applied = SetSecurityInfo(child.hProcess, SE_KERNEL_OBJECT,
        DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, nullptr, nullptr, deniedAcl, nullptr);
    LocalFree(deniedAcl);
    require(applied == ERROR_SUCCESS, "fixture process denial applied");
    const auto unreadable = WindowsProcessIdentity::status(child.dwProcessId, *identity);
    const bool refused = !WindowsProcessIdentity::terminateIfMatches(child.dwProcessId, *identity);
    const auto restored = SetSecurityInfo(child.hProcess, SE_KERNEL_OBJECT,
        DACL_SECURITY_INFORMATION, nullptr, nullptr, originalAcl, nullptr);
    LocalFree(originalDescriptor);
    require(restored == ERROR_SUCCESS, "fixture process ACL restored");
    require(unreadable == WindowsProcessIdentity::Status::Unavailable && refused, "unreadable live identity retained");
    require(WaitForSingleObject(child.hProcess, 0) == WAIT_TIMEOUT, "unreadable identity preserved fixture");
    require(!WindowsProcessIdentity::terminateIfMatches(child.dwProcessId, *identity + 1), "mismatched identity rejected");
    require(WaitForSingleObject(child.hProcess, 0) == WAIT_TIMEOUT, "mismatched identity preserves child");
    require(!WindowsProcessIdentity::terminateIfMatches(child.dwProcessId, 0), "missing expected identity rejected");
    require(WindowsProcessIdentity::terminateIfMatches(child.dwProcessId, *identity, 77), "matching identity permits termination");
    require(WaitForSingleObject(child.hProcess, 5000) == WAIT_OBJECT_0, "child exited");
    require(WindowsProcessIdentity::status(child.dwProcessId, *identity) == WindowsProcessIdentity::Status::Inactive, "exited process inactive");
    DWORD code = 0; require(GetExitCodeProcess(child.hProcess, &code) && code == 77, "termination code preserved");
    require(!WindowsProcessIdentity::terminateIfMatches(child.dwProcessId, *identity), "exited process not terminated again");
    CloseHandle(child.hThread); CloseHandle(child.hProcess);
    child.hThread=nullptr; child.hProcess=nullptr;
    require(WindowsProcessIdentity::status(child.dwProcessId, *identity) == WindowsProcessIdentity::Status::Inactive, "closed exited process identity inactive");
    std::cout << "WindowsProcessIdentityTests passed\n"; return 0;
  } catch (const std::exception &error) {
    if (child.hProcess) { TerminateProcess(child.hProcess, 125); WaitForSingleObject(child.hProcess, 5000); CloseHandle(child.hProcess); }
    if (child.hThread) CloseHandle(child.hThread);
    std::cerr << error.what() << '\n'; return 1;
  }
}

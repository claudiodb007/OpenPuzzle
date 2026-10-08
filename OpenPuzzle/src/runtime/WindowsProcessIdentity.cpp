#include "openpuzzle/runtime/WindowsProcessIdentity.hpp"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
namespace openpuzzle {
namespace {
struct Handle {
  HANDLE value;
  ~Handle() { if (value) CloseHandle(value); }
};
bool missing(std::uint32_t pid) {
  const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) return false;
  Handle owned{snapshot};
  PROCESSENTRY32W entry{};
  entry.dwSize = sizeof(entry);
  if (!Process32FirstW(snapshot, &entry)) return false;
  bool foundSelf = false;
  do {
    if (entry.th32ProcessID == pid) return false;
    if (entry.th32ProcessID == GetCurrentProcessId()) foundSelf = true;
  } while (Process32NextW(snapshot, &entry));
  return foundSelf && GetLastError() == ERROR_NO_MORE_FILES;
}
std::optional<WindowsProcessIdentity::CreationTime> creation(HANDLE process) {
  FILETIME start{}, end{}, kernel{}, user{};
  if (!GetProcessTimes(process, &start, &end, &kernel, &user)) return std::nullopt;
  return (static_cast<std::uint64_t>(start.dwHighDateTime) << 32) | start.dwLowDateTime;
}
}
std::optional<WindowsProcessIdentity::CreationTime>
WindowsProcessIdentity::creationTime(std::uint32_t pid) {
  if (!pid) return std::nullopt;
  Handle process{OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)};
  if (!process.value) return std::nullopt;
  return creation(process.value);
}
WindowsProcessIdentity::Status WindowsProcessIdentity::status(
    std::uint32_t pid, CreationTime expected) {
  if (!pid || !expected) return Status::Unavailable;
  Handle process{OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid)};
  if (!process.value) return missing(pid) ? Status::Inactive : Status::Unavailable;
  const DWORD waited = WaitForSingleObject(process.value, 0);
  if (waited == WAIT_OBJECT_0) return Status::Inactive;
  if (waited != WAIT_TIMEOUT) return Status::Unavailable;
  const auto actual = creation(process.value);
  if (!actual) return WaitForSingleObject(process.value, 0) == WAIT_OBJECT_0
      ? Status::Inactive : Status::Unavailable;
  return *actual == expected ? Status::Running : Status::Inactive;
}
bool WindowsProcessIdentity::terminateIfMatches(std::uint32_t pid,
    CreationTime expected, std::uint32_t exitCode) {
  if (!pid || !expected) return false;
  Handle process{OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE |
                             SYNCHRONIZE, FALSE, pid)};
  if (!process.value || WaitForSingleObject(process.value, 0) != WAIT_TIMEOUT) return false;
  const auto actual = creation(process.value);
  if (!actual || *actual != expected) return false;
  return TerminateProcess(process.value, exitCode) != FALSE;
}
} // namespace openpuzzle

#include "openpuzzle/runtime/WindowsProcessIdentity.hpp"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
namespace openpuzzle {
namespace {
struct Handle {
  HANDLE value;
  ~Handle() { if (value) CloseHandle(value); }
};
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

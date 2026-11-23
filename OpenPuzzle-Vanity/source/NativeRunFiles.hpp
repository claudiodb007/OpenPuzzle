// GPL-3.0-only. Native process ownership and durable, exclusive result files.
#pragma once

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

#ifdef _WIN32
#include "openpuzzle/core/WindowsPrivateFile.hpp"
#include <tlhelp32.h>
#else
#include <fcntl.h>
#include <sys/prctl.h>
#include <unistd.h>
#endif

namespace opvNative {

class Owner {
public:
#ifdef _WIN32
  explicit Owner(std::uint32_t pid)
      : process_(OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION,
                             FALSE, pid)) {
    openpuzzle::windowsFile::Handle snapshot(
        CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool parentMatches = false;
    if (snapshot.valid() && Process32FirstW(snapshot.get(), &entry)) {
      do {
        if (entry.th32ProcessID == GetCurrentProcessId()) {
          parentMatches = entry.th32ParentProcessID == pid;
          break;
        }
      } while (Process32NextW(snapshot.get(), &entry));
    }

    FILETIME parentStart{}, childStart{}, exit{}, kernel{}, user{};
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    if (!parentMatches || !process_.valid() ||
        !GetProcessTimes(process_.get(), &parentStart, &exit, &kernel, &user) ||
        !GetProcessTimes(GetCurrentProcess(), &childStart, &exit, &kernel, &user) ||
        CompareFileTime(&parentStart, &childStart) > 0 ||
        !alive()) {
      throw std::runtime_error("Vanity requires a verified live Windows parent");
    }
    if (!QueryInformationJobObject(nullptr, JobObjectExtendedLimitInformation,
                                   &limits, sizeof(limits), nullptr)) {
      throw std::runtime_error("Cannot verify the Windows engine Job Object (error " +
                               std::to_string(GetLastError()) + ")");
    }
    if (!(limits.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE)) {
      throw std::runtime_error("The Windows engine Job Object is missing kill-on-close");
    }
  }
  bool alive() const {
    return WaitForSingleObject(process_.get(), 0) == WAIT_TIMEOUT;
  }
#else
  explicit Owner(std::uint32_t pid) : pid_(static_cast<pid_t>(pid)) {
    if (prctl(PR_SET_PDEATHSIG, SIGTERM) != 0 || !alive()) {
      throw std::runtime_error("Owner changed");
    }
  }
  bool alive() const { return getppid() == pid_; }
#endif

private:
#ifdef _WIN32
  openpuzzle::windowsFile::Handle process_;
#else
  pid_t pid_;
#endif
};

class Results {
public:
  explicit Results(const std::filesystem::path& path)
#ifdef _WIN32
      : handle_(create(path)) {
    if (!handle_.valid()) throw std::runtime_error("Occupied/unsafe solution file");
  }
#else
      : fd_(open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600)) {
    if (fd_ < 0) throw std::runtime_error("Occupied/unsafe solution file");
  }
  ~Results() { close(fd_); }
#endif
  Results(const Results&) = delete;
  Results& operator=(const Results&) = delete;

  void append(std::string_view record) {
    while (!record.empty()) {
#ifdef _WIN32
      const auto size = static_cast<DWORD>((std::min)(record.size(), std::size_t{4096}));
      DWORD written = 0;
      if (!WriteFile(handle_.get(), record.data(), size, &written, nullptr) || !written) {
        throw std::runtime_error("Solution write failed");
      }
#else
      const auto written = write(fd_, record.data(), record.size());
      if (written < 0 && errno == EINTR) continue;
      if (written <= 0) throw std::runtime_error("Solution write failed");
#endif
      record.remove_prefix(static_cast<std::size_t>(written));
    }
#ifdef _WIN32
    if (!FlushFileBuffers(handle_.get())) throw std::runtime_error("Solution flush failed");
#else
    if (fsync(fd_)) throw std::runtime_error("Solution fsync failed");
#endif
  }

private:
#ifdef _WIN32
  static HANDLE create(const std::filesystem::path& path) {
    namespace files = openpuzzle::windowsFile;
    if (!files::safePath(path)) return INVALID_HANDLE_VALUE;
    files::Security security;
    if (!security.attributes()) return INVALID_HANDLE_VALUE;
    return CreateFileW(path.c_str(), GENERIC_WRITE, 0, security.attributes(),
                       CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT,
                       nullptr);
  }
  openpuzzle::windowsFile::Handle handle_;
#else
  int fd_;
#endif
};

#ifdef _WIN32
inline std::string readTargets(const std::filesystem::path& path) {
  namespace files = openpuzzle::windowsFile;
  if (!files::safePath(path)) throw std::runtime_error("Unsafe target path");
  files::Handle input(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                  nullptr, OPEN_EXISTING,
                                  FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
  BY_HANDLE_FILE_INFORMATION info{};
  constexpr DWORD maxSize = 8192;
  if (!input.valid() || GetFileType(input.get()) != FILE_TYPE_DISK ||
      !GetFileInformationByHandle(input.get(), &info) ||
      (info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) ||
      info.nFileSizeHigh || info.nFileSizeLow > maxSize) {
    throw std::runtime_error("Missing/unsafe/oversized target file");
  }
  std::string contents(info.nFileSizeLow, '\0');
  DWORD bytes = 0;
  if (!ReadFile(input.get(), contents.data(), info.nFileSizeLow, &bytes, nullptr) ||
      bytes != info.nFileSizeLow) throw std::runtime_error("Target file read failed");
  return contents;
}
#endif

} // namespace opvNative

#pragma once

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

#ifdef _WIN32
#include "openpuzzle/core/WindowsPrivateFile.hpp"
#include <atomic>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace openpuzzle {

// Publish a complete private file in its destination directory. A failed write
// or rename leaves the previous destination intact; concurrent writers never
// share a temporary file. This does not make multiple files one transaction.
inline bool writePrivateFileAtomically(
    const std::filesystem::path& path,
    const std::string& contents) {
#ifdef _WIN32
  using namespace windowsFile;
  if (!safePath(path)) return false;
  Security security;
  if (!security.attributes()) return false;
  const DWORD attributes = GetFileAttributesW(path.c_str());
  if (attributes != INVALID_FILE_ATTRIBUTES &&
      (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_READONLY))) return false;
  if (attributes == INVALID_FILE_ATTRIBUTES && GetLastError() != ERROR_FILE_NOT_FOUND) return false;
  static std::atomic<unsigned long long> sequence{0};
  std::filesystem::path temporary;
  HANDLE raw = INVALID_HANDLE_VALUE;
  for (int attempt = 0; attempt < 64; ++attempt) {
    temporary = path;
    temporary += L".tmp." + std::to_wstring(GetCurrentProcessId()) + L"." +
                 std::to_wstring(sequence++) + L"." + std::to_wstring(GetTickCount64());
    raw = CreateFileW(temporary.c_str(), GENERIC_WRITE | READ_CONTROL | WRITE_DAC,
                      0, security.attributes(), CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (raw != INVALID_HANDLE_VALUE) break;
    if (GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_ALREADY_EXISTS) return false;
  }
  if (raw == INVALID_HANDLE_VALUE) return false;
  Handle output(raw);
  bool success = security.protect(raw);
  std::size_t position = 0;
  while (success && position < contents.size()) {
    DWORD written = 0;
    const auto count = static_cast<DWORD>((std::min)(contents.size() - position,
                                      static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())));
    success = WriteFile(raw, contents.data() + position, count, &written, nullptr) && written > 0;
    if (success) position += written;
  }
  if (success) success = FlushFileBuffers(raw) != 0;
  if (!output.close()) success = false;
  if (success) success = MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
  if (!success) DeleteFileW(temporary.c_str());
  return success;
#else
  std::string pattern = path.string() + ".tmp.XXXXXX";
  std::vector<char> temporary(pattern.begin(), pattern.end());
  temporary.push_back('\0');
  const int descriptor = ::mkstemp(temporary.data());
  if (descriptor < 0) {
    return false;
  }

  bool success = ::fchmod(descriptor, S_IRUSR | S_IWUSR) == 0;
  std::size_t position = 0;
  while (success && position < contents.size()) {
    const std::size_t remaining = contents.size() - position;
    const std::size_t size = remaining < static_cast<std::size_t>(
        std::numeric_limits<ssize_t>::max())
        ? remaining : static_cast<std::size_t>(std::numeric_limits<ssize_t>::max());
    const ssize_t written = ::write(
        descriptor, contents.data() + position, size);
    if (written < 0 && errno == EINTR) {
      continue;
    }
    if (written <= 0) {
      success = false;
    } else {
      position += static_cast<std::size_t>(written);
    }
  }
  if (success) {
    success = ::fsync(descriptor) == 0;
  }
  if (::close(descriptor) != 0) {
    success = false;
  }
  if (success) {
    success = ::rename(temporary.data(), path.c_str()) == 0;
  }
  if (!success) {
    ::unlink(temporary.data());
  }
  return success;
#endif
}

} // namespace openpuzzle

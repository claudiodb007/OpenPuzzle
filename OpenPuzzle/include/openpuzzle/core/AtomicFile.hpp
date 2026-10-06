#pragma once

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

namespace openpuzzle {

// Publish a complete private file in its destination directory. A failed write
// or rename leaves the previous destination intact; concurrent writers never
// share a temporary file. This does not make multiple files one transaction.
inline bool writePrivateFileAtomically(
    const std::filesystem::path& path,
    const std::string& contents) {
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
}

} // namespace openpuzzle

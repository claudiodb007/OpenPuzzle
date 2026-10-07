#include "openpuzzle/runtime/WorkspaceSecurity.hpp"

#ifdef _WIN32
#include "openpuzzle/core/WindowsPrivateFile.hpp"
#endif
#include <stdexcept>
#include <system_error>

namespace openpuzzle {

void WorkspaceSecurity::prepare(
    const std::filesystem::path &workspace) {
  if (workspace.empty()) {
    throw std::invalid_argument(
        "Workspace path is empty");
  }

#ifdef _WIN32
  if (!windowsFile::safePath(workspace)) throw std::runtime_error("Unsafe workspace path");
  windowsFile::Security security;
  if (!security.attributes()) throw std::runtime_error("Unable to create private security descriptor");
  auto current = workspace.root_path();
  for (const auto& part : workspace.relative_path()) {
    current /= part;
    if (GetFileAttributesW(current.c_str()) == INVALID_FILE_ATTRIBUTES) {
      if (!CreateDirectoryW(current.c_str(), security.attributes()) && GetLastError() != ERROR_ALREADY_EXISTS)
        throw std::runtime_error("Unable to create private workspace");
    }
  }
  windowsFile::Handle directory(CreateFileW(workspace.c_str(), READ_CONTROL | WRITE_DAC,
      FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
  if (!directory.valid() || !security.protect(directory.get(), true))
    throw std::runtime_error("Unable to protect workspace");
#else
  std::error_code error;

  std::filesystem::create_directories(
      workspace,
      error);

  if (error) {
    throw std::runtime_error(
        "Unable to create private workspace: " +
        error.message());
  }

  std::filesystem::permissions(
      workspace,
      std::filesystem::perms::owner_all,
      std::filesystem::perm_options::replace,
      error);

  if (error) {
    throw std::runtime_error(
        "Unable to protect workspace: " +
        error.message());
  }
#endif
}

void WorkspaceSecurity::protectFile(
    const std::filesystem::path &file) {
  if (file.empty()) {
    throw std::invalid_argument(
        "File path is empty");
  }

#ifdef _WIN32
  if (!windowsFile::safePath(file)) throw std::runtime_error("Unsafe file path");
  windowsFile::Security security;
  windowsFile::Handle input(CreateFileW(file.c_str(), READ_CONTROL | WRITE_DAC,
      FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
  if (!input.valid() || !security.protect(input.get())) throw std::runtime_error("Unable to protect file");
#else
  std::error_code error;

  if (!std::filesystem::is_regular_file(
          file,
          error) ||
      error) {
    throw std::runtime_error(
        "Unable to protect missing file: " +
        file.string());
  }

  std::filesystem::permissions(
      file,
      std::filesystem::perms::owner_read |
          std::filesystem::perms::owner_write,
      std::filesystem::perm_options::replace,
      error);

  if (error) {
    throw std::runtime_error(
        "Unable to protect file: " +
        error.message());
  }
#endif
}

} // namespace openpuzzle

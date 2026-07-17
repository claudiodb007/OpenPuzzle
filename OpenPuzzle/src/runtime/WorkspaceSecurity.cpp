#include "openpuzzle/runtime/WorkspaceSecurity.hpp"

#include <stdexcept>
#include <system_error>

namespace openpuzzle {

void WorkspaceSecurity::prepare(
    const std::filesystem::path &workspace) {
  if (workspace.empty()) {
    throw std::invalid_argument(
        "Workspace path is empty");
  }

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
}

void WorkspaceSecurity::protectFile(
    const std::filesystem::path &file) {
  if (file.empty()) {
    throw std::invalid_argument(
        "File path is empty");
  }

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
}

} // namespace openpuzzle

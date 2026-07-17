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

} // namespace openpuzzle

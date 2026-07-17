#include "openpuzzle/runtime/WorkspaceSecurity.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

using namespace openpuzzle;

namespace {

void assertPrivate(
    const std::filesystem::path &workspace) {
  const auto permissions =
      std::filesystem::status(
          workspace).permissions();

  assert(
      (permissions &
       std::filesystem::perms::owner_all) ==
      std::filesystem::perms::owner_all);

  assert(
      (permissions &
       std::filesystem::perms::group_all) ==
      std::filesystem::perms::none);

  assert(
      (permissions &
       std::filesystem::perms::others_all) ==
      std::filesystem::perms::none);
}

void assertPrivateFile(
    const std::filesystem::path &file) {
  const auto permissions =
      std::filesystem::status(
          file).permissions();

  assert(
      (permissions &
       std::filesystem::perms::owner_read) !=
      std::filesystem::perms::none);

  assert(
      (permissions &
       std::filesystem::perms::owner_write) !=
      std::filesystem::perms::none);

  assert(
      (permissions &
       std::filesystem::perms::group_all) ==
      std::filesystem::perms::none);

  assert(
      (permissions &
       std::filesystem::perms::others_all) ==
      std::filesystem::perms::none);
}

} // namespace

int main() {
  const auto root =
      std::filesystem::temp_directory_path() /
      (
          "openpuzzle-workspace-security-" +
          std::to_string(getpid())
      );

  const auto workspace =
      root /
      "assignments" /
      "test-assignment";

  std::filesystem::remove_all(
      root);

  /*
   * Um workspace novo é criado diretamente
   * com proteção exclusiva do proprietário.
   */
  WorkspaceSecurity::prepare(
      workspace);

  assert(
      std::filesystem::is_directory(
          workspace));

  assertPrivate(
      workspace);

  /*
   * Um diretório existente e permissivo também
   * deve ser restringido.
   */
  std::filesystem::permissions(
      workspace,
      std::filesystem::perms::all,
      std::filesystem::perm_options::replace);

  WorkspaceSecurity::prepare(
      workspace);

  assertPrivate(
      workspace);

  /*
   * A operação é idempotente.
   */
  WorkspaceSecurity::prepare(
      workspace);

  assertPrivate(
      workspace);

  const auto sensitiveFile =
      workspace /
      "found.txt";

  {
    std::ofstream output(
        sensitiveFile);

    output
        << "synthetic-secret\n";
  }

  std::filesystem::permissions(
      sensitiveFile,
      std::filesystem::perms::all,
      std::filesystem::perm_options::replace);

  WorkspaceSecurity::protectFile(
      sensitiveFile);

  assertPrivateFile(
      sensitiveFile);

  std::filesystem::remove_all(
      root);

  std::cout
      << "WorkspaceSecurityTests passed\n";

  return 0;
}

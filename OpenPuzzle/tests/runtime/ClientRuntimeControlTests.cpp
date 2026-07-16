#include "openpuzzle/runtime/ClientRuntimeControl.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

using namespace openpuzzle;

int main() {
  const char *originalHome =
      std::getenv("HOME");

  const bool hadHome =
      originalHome != nullptr;

  const std::string savedHome =
      hadHome
          ? originalHome
          : "";

  const auto temporaryHome =
      std::filesystem::temp_directory_path() /
      (
          "openpuzzle-runtime-control-" +
          std::to_string(getpid())
      );

  std::filesystem::remove_all(
      temporaryHome);

  std::filesystem::create_directories(
      temporaryHome);

  assert(
      setenv(
          "HOME",
          temporaryHome.string().c_str(),
          1) == 0);

  assert(!ClientRuntimeControl::running());
  assert(!ClientRuntimeControl::runtimePid());

  assert(ClientRuntimeControl::acquire());
  assert(ClientRuntimeControl::running());

  const auto pid =
      ClientRuntimeControl::runtimePid();

  assert(pid);
  assert(*pid == static_cast<int>(getpid()));

  /*
   * A mesma ou outra instância não pode adquirir
   * o controlo enquanto o PID estiver ativo.
   */
  assert(!ClientRuntimeControl::acquire());

  assert(ClientRuntimeControl::release());
  assert(!ClientRuntimeControl::runtimePid());
  assert(!ClientRuntimeControl::running());

  /*
   * Um PID obsoleto é limpo automaticamente.
   */
  std::filesystem::create_directories(
      ClientRuntimeControl::pidPath()
          .parent_path());

  {
    std::ofstream output(
        ClientRuntimeControl::pidPath());

    output << "999999999\n";
  }

  assert(!ClientRuntimeControl::requestStop());
  assert(!ClientRuntimeControl::runtimePid());

  assert(ClientRuntimeControl::acquire());
  assert(ClientRuntimeControl::release());

  std::filesystem::remove_all(
      temporaryHome);

  if (hadHome) {
    assert(
        setenv(
            "HOME",
            savedHome.c_str(),
            1) == 0);
  } else {
    assert(
        unsetenv("HOME") == 0);
  }

  std::cout
      << "ClientRuntimeControlTests passed\n";

  return 0;
}

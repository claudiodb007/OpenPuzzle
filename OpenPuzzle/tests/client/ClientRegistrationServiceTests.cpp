#include "openpuzzle/client/ClientRegistrationService.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <regex>
#include <string>
#include <unistd.h>

using namespace openpuzzle::client;

namespace {

bool isUuidV4(
    const std::string& value) {
  const std::regex expression(
      "^[0-9a-f]{8}-"
      "[0-9a-f]{4}-"
      "4[0-9a-f]{3}-"
      "[89ab][0-9a-f]{3}-"
      "[0-9a-f]{12}$",
      std::regex::icase);

  return std::regex_match(
      value,
      expression);
}

} // namespace

int main() {
  const char* originalHome =
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
          "openpuzzle-client-registration-" +
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

  const auto first =
      ClientRegistrationService::
          collectLocalRegistration();

  assert(first.valid());
  assert(isUuidV4(first.clientId));

  const auto second =
      ClientRegistrationService::
          collectLocalRegistration();

  assert(
      second.clientId ==
      first.clientId);

  ClientRegistrationService service;

  const auto failed =
      service.registerWith(
          "http://127.0.0.1:1");

  assert(!failed.success);
  assert(!failed.error.empty());

  assert(
      failed.registration.clientId ==
      first.clientId);

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
      << "ClientRegistrationServiceTests passed\n";

  return 0;
}

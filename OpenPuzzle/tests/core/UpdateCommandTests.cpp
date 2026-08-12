#include "openpuzzle/core/commands/UpdateCommand.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace openpuzzle;

int main() {
  const std::string hash =
      "3a3aadb56cf1f3d83a484f704edc69e432cb1c4be30b6acc736a16d7aa8876c4";

  {
    std::string error;
    const auto release = UpdateCommand::parseReleaseManifest(
        hash + "  OpenPuzzle-1.0.9-portable-3a3aadb5.deb\n", error);
    assert(release);
    assert(release->sha256 == hash);
    assert(release->filename == "OpenPuzzle-1.0.9-portable-3a3aadb5.deb");
    assert(release->version == "1.0.9");
    assert(error.empty());
  }

  {
    std::string error;
    const auto release = UpdateCommand::parseReleaseManifest(
        hash + "  ../../OpenPuzzle-1.0.9-portable-3a3aadb5.deb\n", error);
    assert(!release);
    assert(!error.empty());
  }

  {
    std::string error;
    const auto release = UpdateCommand::parseReleaseManifest(
        hash + "  OpenPuzzle-1.0.9-portable-deadbeef.deb\n", error);
    assert(!release);
    assert(error.find("SHA-256 prefix") != std::string::npos);
  }

  assert(UpdateCommand::compareVersions("1.0.10", "1.0.9") > 0);
  assert(UpdateCommand::compareVersions("1.1.0", "1.0.99") > 0);
  assert(UpdateCommand::compareVersions("2.0.0", "10.0.0") < 0);
  assert(UpdateCommand::compareVersions("1.0.10", "1.0.10") == 0);

  bool rejected = false;
  try {
    (void)UpdateCommand::compareVersions("1.0", "1.0.0");
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  assert(rejected);

  {
    std::string error;
    const auto options = UpdateCommand::parseOptions({}, error);
    assert(options);
    assert(!options->checkOnly);
    assert(!options->downloadOnly);
    assert(!options->safe);
    assert(error.empty());
  }

  {
    std::string error;
    const auto options = UpdateCommand::parseOptions({"--safe"}, error);
    assert(options);
    assert(options->safe);
    assert(!options->checkOnly);
    assert(!options->downloadOnly);
  }

  {
    std::string error;
    const auto options = UpdateCommand::parseOptions(
        {"--check", "--safe"}, error);
    assert(!options);
    assert(error.find("cannot be combined") != std::string::npos);
  }

  {
    std::string error;
    const auto options = UpdateCommand::parseOptions(
        {"--download-only", "--safe"}, error);
    assert(!options);
    assert(error.find("cannot be combined") != std::string::npos);
  }

  {
    std::string error;
    const auto options = UpdateCommand::parseOptions(
        {"--unknown"}, error);
    assert(!options);
    assert(error.find("unknown option") != std::string::npos);
  }

  std::cout << "UpdateCommandTests passed\n";
  return 0;
}

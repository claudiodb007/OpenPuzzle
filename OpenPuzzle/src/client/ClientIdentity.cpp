#include "openpuzzle/client/ClientIdentity.hpp"

#include "openpuzzle/runtime/WorkspaceSecurity.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>

namespace openpuzzle::client {

namespace {

std::filesystem::path identityPath() {
  const char* home =
      std::getenv("HOME");

  std::filesystem::path root =
      home
          ? std::filesystem::path(home)
          : std::filesystem::current_path();

  return root /
         ".config" /
         "OpenPuzzle" /
         "client.id";
}

std::string generateUuid() {
  std::random_device randomDevice;
  std::mt19937_64 generator(
      randomDevice());

  std::uniform_int_distribution<unsigned int>
      distribution(0, 255);

  unsigned char bytes[16];

  for (auto& byte : bytes) {
    byte = static_cast<unsigned char>(
        distribution(generator));
  }

  /*
   * UUID versão 4.
   */
  bytes[6] =
      static_cast<unsigned char>(
          (bytes[6] & 0x0f) | 0x40);

  bytes[8] =
      static_cast<unsigned char>(
          (bytes[8] & 0x3f) | 0x80);

  std::ostringstream output;

  output
      << std::hex
      << std::setfill('0');

  for (int index = 0;
       index < 16;
       ++index) {
    output
        << std::setw(2)
        << static_cast<int>(
               bytes[index]);

    if (index == 3 ||
        index == 5 ||
        index == 7 ||
        index == 9) {
      output << '-';
    }
  }

  return output.str();
}

} // namespace

std::string ClientIdentity::loadOrCreate() {
  const auto path =
      identityPath();

  try {
    WorkspaceSecurity::prepare(
        path.parent_path());

    if (std::filesystem::is_regular_file(
            path)) {
      WorkspaceSecurity::protectFile(
          path);
    }
  } catch (...) {
    return {};
  }

  {
    std::ifstream input(path);

    std::string identity;

    if (input &&
        std::getline(
            input,
            identity) &&
        !identity.empty()) {
      return identity;
    }
  }

  const std::string identity =
      generateUuid();

  std::ofstream output(
      path,
      std::ios::trunc);

  if (!output) {
    return {};
  }

  try {
    WorkspaceSecurity::protectFile(
        path);
  } catch (...) {
    return {};
  }

  output
      << identity
      << '\n';

  output.close();

  return output
             ? identity
             : std::string{};
}

} // namespace openpuzzle::client

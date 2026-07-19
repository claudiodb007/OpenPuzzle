#include "openpuzzle/tools/ToolManager.hpp"

#include "openpuzzle/config/ConfigurationManager.hpp"

#include <array>
#include <cerrno>
#include <filesystem>
#include <optional>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

namespace openpuzzle {

namespace {

constexpr const char *BundledBackend =
    OPENPUZZLE_BUNDLED_BACKEND;

constexpr const char *BundledEngineName =
    OPENPUZZLE_BUNDLED_ENGINE_NAME;

constexpr const char *BundledEngineIdentity =
    OPENPUZZLE_BUNDLED_ENGINE_IDENTITY;

bool executableFile(
    const fs::path &path) {
  return
      fs::is_regular_file(path) &&
      access(path.c_str(), X_OK) == 0;
}

std::string trimRecord(
    std::string value) {
  while (
      !value.empty() &&
      (
          value.back() == '\r' ||
          value.back() == '\n'
      )) {
    value.pop_back();
  }

  return value;
}

std::optional<std::string> engineIdentity(
    const fs::path &path) {
  int descriptors[2];

  if (pipe(descriptors) != 0) {
    return std::nullopt;
  }

  const pid_t pid = fork();

  if (pid < 0) {
    close(descriptors[0]);
    close(descriptors[1]);
    return std::nullopt;
  }

  if (pid == 0) {
    close(descriptors[0]);

    dup2(descriptors[1], STDOUT_FILENO);
    dup2(descriptors[1], STDERR_FILENO);
    close(descriptors[1]);

    const std::string executable =
        path.string();

    const std::string argumentZero =
        path.filename().string();

    execl(
        executable.c_str(),
        argumentZero.c_str(),
        "--openpuzzle-engine-version",
        static_cast<char *>(nullptr));

    _exit(127);
  }

  close(descriptors[1]);

  std::string output;
  std::array<char, 256> buffer{};

  while (output.size() < 4096) {
    const ssize_t count = read(
        descriptors[0],
        buffer.data(),
        buffer.size());

    if (count > 0) {
      output.append(
          buffer.data(),
          static_cast<std::size_t>(count));
      continue;
    }

    if (count < 0 && errno == EINTR) {
      continue;
    }

    break;
  }

  close(descriptors[0]);

  int status = 0;

  if (waitpid(pid, &status, 0) != pid ||
      !WIFEXITED(status) ||
      WEXITSTATUS(status) != 0) {
    return std::nullopt;
  }

  return trimRecord(output);
}

std::optional<fs::path> runningExecutable() {
  std::array<char, 4096> buffer{};

  const ssize_t size = readlink(
      "/proc/self/exe",
      buffer.data(),
      buffer.size() - 1);

  if (size <= 0) {
    return std::nullopt;
  }

  buffer[static_cast<std::size_t>(size)] = '\0';

  return fs::path(buffer.data());
}

} // namespace

std::string ToolManager::configPath() {
  return ConfigurationManager::configPath();
}

bool ToolManager::configureBitCrack(
    const std::string &) {
  return false;
}

bool ToolManager::configureBitCrack(
    const std::string &,
    const std::string &) {
  return false;
}

std::string ToolManager::bundledBackend() {
  return BundledBackend;
}

bool ToolManager::validateBitCrackEngine(
    const std::string &path,
    const std::string &backend,
    std::string *error) {
  const auto fail =
      [error](const std::string &message) {
        if (error != nullptr) {
          *error = message;
        }

        return false;
      };

  if (backend != BundledBackend) {
    return fail(
        "Engine backend does not match this "
        "OpenPuzzle package");
  }

  const fs::path executable(path);

  if (!executableFile(executable)) {
    return fail(
        "Bundled engine is missing or not executable");
  }

  const auto identity =
      engineIdentity(executable);

  if (!identity ||
      *identity != BundledEngineIdentity) {
    return fail(
        "Bundled engine identity was rejected");
  }

  if (error != nullptr) {
    error->clear();
  }

  return true;
}

std::optional<std::string>
ToolManager::bundledBitCrackPath() {
  const auto executable =
      runningExecutable();

  if (!executable) {
    return std::nullopt;
  }

  const auto directory =
      executable->parent_path();

  const std::vector<fs::path> candidates = {
      directory /
          "libexec" /
          "OpenPuzzle" /
          BundledEngineName,

      directory.parent_path() /
          "libexec" /
          "OpenPuzzle" /
          BundledEngineName,
  };

  for (const auto &candidate : candidates) {
    if (validateBitCrackEngine(
            candidate.string(),
            BundledBackend)) {
      return candidate.string();
    }
  }

  return std::nullopt;
}

std::optional<std::string> ToolManager::bitcrackPath() {
  return bundledBitCrackPath();
}

std::optional<std::string> ToolManager::bitcrackCudaPath() {
  if (bundledBackend() != "cuda") {
    return std::nullopt;
  }

  return bundledBitCrackPath();
}

std::optional<std::string> ToolManager::bitcrackOpenCLPath() {
  if (bundledBackend() != "opencl") {
    return std::nullopt;
  }

  return bundledBitCrackPath();
}

} // namespace openpuzzle

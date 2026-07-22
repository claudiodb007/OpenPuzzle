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

constexpr const char *CudaEngineName =
    OPENPUZZLE_CUDA_ENGINE_NAME;

constexpr const char *OpenClEngineName =
    OPENPUZZLE_OPENCL_ENGINE_NAME;

constexpr const char *KeyHuntEngineName =
    OPENPUZZLE_KEYHUNT_ENGINE_NAME;

constexpr const char *CudaEngineIdentity =
    OPENPUZZLE_CUDA_ENGINE_IDENTITY;

constexpr const char *OpenClEngineIdentity =
    OPENPUZZLE_OPENCL_ENGINE_IDENTITY;

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

std::optional<std::string> commandOutput(
    const fs::path &path,
    const std::string &argument) {
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
        argument.c_str(),
        static_cast<char *>(nullptr));

    _exit(127);
  }

  close(descriptors[1]);

  std::string output;
  std::array<char, 512> buffer{};

  while (output.size() < 65536) {
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

std::optional<std::string> bundledExecutable(
    const char *name) {
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
          name,

      directory.parent_path() /
          "libexec" /
          "OpenPuzzle" /
          name,
  };

  for (const auto &candidate : candidates) {
    if (executableFile(candidate)) {
      return candidate.string();
    }
  }

  return std::nullopt;
}

const char *engineName(
    const std::string &backend) {
  if (backend == "cuda") {
    return CudaEngineName;
  }

  if (backend == "opencl") {
    return OpenClEngineName;
  }

  return nullptr;
}

const char *expectedIdentity(
    const std::string &backend) {
  if (backend == "cuda") {
    return CudaEngineIdentity;
  }

  if (backend == "opencl") {
    return OpenClEngineIdentity;
  }

  return nullptr;
}

bool engineHasDevices(
    const std::optional<std::string> &path) {
  if (!path) {
    return false;
  }

  const auto output = commandOutput(
      *path,
      "--list-devices");

  return
      output &&
      output->find("ID:") != std::string::npos;
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

bool ToolManager::supportsBackend(
    const std::string &backend) {
  return
      backend == "cuda" ||
      backend == "opencl" ||
      backend == "cpu";
}

std::vector<std::string>
ToolManager::bundledBackends() {
  return {"cuda", "opencl", "cpu"};
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

  const char *identity =
      expectedIdentity(backend);

  if (identity == nullptr) {
    return fail(
        "Unsupported bundled engine backend");
  }

  const fs::path executable(path);

  if (!executableFile(executable)) {
    return fail(
        "Bundled engine is missing or not executable");
  }

  const auto actualIdentity = commandOutput(
      executable,
      "--openpuzzle-engine-version");

  if (!actualIdentity ||
      *actualIdentity != identity) {
    return fail(
        "Bundled engine identity was rejected");
  }

  if (error != nullptr) {
    error->clear();
  }

  return true;
}

std::optional<std::string>
ToolManager::bundledBitCrackPath(
    const std::string &backend) {
  const char *name = engineName(backend);

  if (name == nullptr) {
    return std::nullopt;
  }

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
          name,

      directory.parent_path() /
          "libexec" /
          "OpenPuzzle" /
          name,
  };

  for (const auto &candidate : candidates) {
    if (validateBitCrackEngine(
            candidate.string(),
            backend)) {
      return candidate.string();
    }
  }

  return std::nullopt;
}

std::optional<std::string>
ToolManager::bitcrackCudaPath() {
  return bundledBitCrackPath("cuda");
}

std::optional<std::string>
ToolManager::bitcrackOpenCLPath() {
  return bundledBitCrackPath("opencl");
}

std::string ToolManager::preferredBackend() {
  if (engineHasDevices(
          bitcrackCudaPath())) {
    return "cuda";
  }

  if (engineHasDevices(
          bitcrackOpenCLPath())) {
    return "opencl";
  }

  /*
   * This fallback keeps informational and isolated
   * test commands deterministic on GPU-less hosts.
   * Real setup/benchmark still fails at discovery.
   */
  if (bitcrackCudaPath()) {
    return "cuda";
  }

  if (bitcrackOpenCLPath()) {
    return "opencl";
  }

  return {};
}

std::string ToolManager::bundledBackend() {
  return preferredBackend();
}

std::optional<std::string>
ToolManager::bundledBitCrackPath() {
  const std::string backend =
      preferredBackend();

  if (backend.empty()) {
    return std::nullopt;
  }

  return bundledBitCrackPath(backend);
}

std::optional<std::string>
ToolManager::bitcrackPath() {
  return bundledBitCrackPath();
}

std::optional<std::string>
ToolManager::keyhuntPath() {
  return bundledExecutable(
      KeyHuntEngineName);
}

} // namespace openpuzzle

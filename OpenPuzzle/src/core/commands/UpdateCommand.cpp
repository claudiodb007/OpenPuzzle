#include "openpuzzle/core/commands/UpdateCommand.hpp"

#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/runtime/ClientRuntimeControl.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

namespace openpuzzle {
namespace {

constexpr const char* kManifestUrl =
    "https://github.com/claudiodb007/OpenPuzzle/"
    "releases/latest/download/SHA256SUMS.txt";

struct ProcessResult {
  int exitCode = 1;
  std::string output;
};

std::string trim(std::string value) {
  while (!value.empty() &&
         std::isspace(static_cast<unsigned char>(value.front()))) {
    value.erase(value.begin());
  }
  while (!value.empty() &&
         std::isspace(static_cast<unsigned char>(value.back()))) {
    value.pop_back();
  }
  return value;
}

std::string lowercase(std::string value) {
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
      });
  return value;
}

ProcessResult runProcess(
    const std::vector<std::string>& arguments,
    bool capture) {
  if (arguments.empty()) {
    return {1, "empty command"};
  }

  int outputPipe[2] = {-1, -1};
  if (capture && pipe(outputPipe) != 0) {
    return {1, "unable to create output pipe"};
  }

  const pid_t child = fork();
  if (child < 0) {
    if (capture) {
      close(outputPipe[0]);
      close(outputPipe[1]);
    }
    return {1, "unable to start process"};
  }

  if (child == 0) {
    if (capture) {
      close(outputPipe[0]);
      dup2(outputPipe[1], STDOUT_FILENO);
      dup2(outputPipe[1], STDERR_FILENO);
      close(outputPipe[1]);
    }

    std::vector<char*> argv;
    argv.reserve(arguments.size() + 1);
    for (const auto& argument : arguments) {
      argv.push_back(const_cast<char*>(argument.c_str()));
    }
    argv.push_back(nullptr);
    execvp(argv.front(), argv.data());
    _exit(127);
  }

  std::string output;
  if (capture) {
    close(outputPipe[1]);
    std::array<char, 4096> buffer{};
    while (true) {
      const ssize_t count =
          read(outputPipe[0], buffer.data(), buffer.size());
      if (count > 0) {
        output.append(buffer.data(), static_cast<std::size_t>(count));
        continue;
      }
      if (count < 0 && errno == EINTR) {
        continue;
      }
      break;
    }
    close(outputPipe[0]);
  }

  int status = 0;
  while (waitpid(child, &status, 0) < 0) {
    if (errno != EINTR) {
      return {1, output};
    }
  }

  if (WIFEXITED(status)) {
    return {WEXITSTATUS(status), output};
  }
  return {1, output};
}

bool commandExists(const std::string& command) {
  const char* pathValue = std::getenv("PATH");
  if (!pathValue) {
    return false;
  }
  std::istringstream paths(pathValue);
  std::string directory;
  while (std::getline(paths, directory, ':')) {
    if (directory.empty()) {
      directory = ".";
    }
    const fs::path candidate = fs::path(directory) / command;
    if (access(candidate.c_str(), X_OK) == 0) {
      return true;
    }
  }
  return false;
}

bool processExists(int pid) {
  if (pid <= 0) {
    return false;
  }
  if (kill(pid, 0) == 0) {
    return true;
  }
  return errno == EPERM;
}

std::optional<std::string> activeExecution() {
  static const std::array<const char*, 5> slots = {
      "primary", "gpu", "cpu", "cuda", "opencl"};

  for (const char* slot : slots) {
    const auto state = client::ClientStateStore::load(slot);
    if (state && processExists(state->pid)) {
      std::ostringstream description;
      description << "slot " << slot << ", PID " << state->pid;
      if (!state->assignmentId.empty()) {
        description << ", assignment " << state->assignmentId;
      }
      return description.str();
    }

    if (ClientRuntimeControl::running(slot)) {
      std::ostringstream description;
      description << "slot " << slot << ", runtime marker active";
      if (const auto runtimePid =
              ClientRuntimeControl::runtimePid(slot)) {
        description << ", PID " << *runtimePid;
      }
      return description.str();
    }
  }
  return std::nullopt;
}

void printError(
    const std::string& code,
    const std::string& problem,
    const std::string& action) {
  std::cerr
      << "Update failed\n"
      << "Code............... " << code << '\n'
      << "Problem............ " << problem << '\n'
      << "Action............. " << action << '\n';
}

std::optional<std::array<int, 3>> versionParts(
    const std::string& version) {
  static const std::regex pattern(R"(^([0-9]+)[.]([0-9]+)[.]([0-9]+)$)");
  std::smatch match;
  if (!std::regex_match(version, match, pattern)) {
    return std::nullopt;
  }
  try {
    return std::array<int, 3>{
        std::stoi(match[1].str()),
        std::stoi(match[2].str()),
        std::stoi(match[3].str())};
  } catch (...) {
    return std::nullopt;
  }
}

bool privateDirectory(const fs::path& path) {
  std::error_code error;
  if (fs::exists(path, error) && fs::is_symlink(path, error)) {
    return false;
  }
  fs::create_directories(path, error);
  if (error) {
    return false;
  }
  fs::permissions(
      path,
      fs::perms::owner_all,
      fs::perm_options::replace,
      error);
  return !error;
}

bool safeDownloadedFile(const fs::path& path) {
  std::error_code error;
  if (!fs::is_regular_file(path, error) || error) {
    return false;
  }
  fs::permissions(
      path,
      fs::perms::owner_read | fs::perms::owner_write,
      fs::perm_options::replace,
      error);
  return !error;
}

std::string currentVersion() {
#ifdef OPENPUZZLE_VERSION
  return OPENPUZZLE_VERSION;
#else
  return "0.0.0";
#endif
}

} // namespace

std::optional<UpdateRelease>
UpdateCommand::parseReleaseManifest(
    const std::string& manifest,
    std::string& error) {
  static const std::regex filenamePattern(
      R"(^OpenPuzzle-([0-9]+[.][0-9]+[.][0-9]+)-portable-([0-9a-f]{8})[.]deb$)");
  static const std::regex hashPattern(R"(^[0-9a-f]{64}$)");

  std::optional<UpdateRelease> release;
  std::istringstream lines(manifest);
  std::string line;
  while (std::getline(lines, line)) {
    std::istringstream fields(line);
    std::string hash;
    std::string filename;
    std::string extra;
    if (!(fields >> hash >> filename) || (fields >> extra)) {
      continue;
    }

    hash = lowercase(hash);
    std::smatch match;
    if (!std::regex_match(hash, hashPattern) ||
        !std::regex_match(filename, match, filenamePattern)) {
      continue;
    }
    if (hash.substr(0, 8) != match[2].str()) {
      error = "package filename does not match its SHA-256 prefix";
      return std::nullopt;
    }
    if (release) {
      error = "manifest contains more than one portable package";
      return std::nullopt;
    }
    release = UpdateRelease{hash, filename, match[1].str()};
  }

  if (!release) {
    error = "manifest does not contain one valid portable package";
  }
  return release;
}

int UpdateCommand::compareVersions(
    const std::string& left,
    const std::string& right) {
  const auto leftParts = versionParts(left);
  const auto rightParts = versionParts(right);
  if (!leftParts || !rightParts) {
    throw std::invalid_argument("version must use MAJOR.MINOR.PATCH");
  }
  if (*leftParts < *rightParts) {
    return -1;
  }
  if (*rightParts < *leftParts) {
    return 1;
  }
  return 0;
}

int UpdateCommand::run(
    const std::vector<std::string>& args) const {
  bool checkOnly = false;
  bool downloadOnly = false;
  for (const auto& argument : args) {
    if (argument == "--check") {
      checkOnly = true;
    } else if (argument == "--download-only") {
      downloadOnly = true;
    } else {
      printError(
          "OP-UPDATE-000",
          "unknown option: " + argument,
          "Use: openpuzzle update [--check|--download-only]");
      return 1;
    }
  }
  if (checkOnly && downloadOnly) {
    printError(
        "OP-UPDATE-000",
        "--check and --download-only cannot be combined",
        "Choose only one update mode.");
    return 1;
  }

  if (!checkOnly && !downloadOnly) {
    if (const auto active = activeExecution()) {
      printError(
          "OP-UPDATE-001",
          "an OpenPuzzle execution is active (" + *active + ")",
          "Run 'openpuzzle safestop', wait for it to finish, then rerun 'openpuzzle update'.");
      return 1;
    }
  }

  std::vector<std::string> requiredCommands = {"curl"};
  if (!checkOnly) {
    requiredCommands.emplace_back("sha256sum");
    requiredCommands.emplace_back("dpkg-deb");
    if (!downloadOnly) {
      requiredCommands.emplace_back("apt-get");
    }
  }

  for (const auto& command : requiredCommands) {
    if (!commandExists(command)) {
      printError(
          "OP-UPDATE-002",
          std::string("required command is missing: ") + command,
          "Install the missing system command and rerun 'openpuzzle update'.");
      return 1;
    }
  }

  const char* homeValue = std::getenv("HOME");
  if (!homeValue || !*homeValue) {
    printError("OP-UPDATE-002", "HOME is not defined", "Define HOME and retry.");
    return 1;
  }

  const fs::path updateRoot =
      fs::path(homeValue) / ".cache" / "OpenPuzzle" / "updates";
  const fs::path workDirectory =
      updateRoot / ("pending-" + std::to_string(getpid()));
  if (!privateDirectory(workDirectory)) {
    printError(
        "OP-UPDATE-002",
        "unable to create a private update directory",
        "Check permissions under ~/.cache/OpenPuzzle and retry.");
    return 1;
  }

  struct Cleanup {
    fs::path path;
    bool preserve = false;
    ~Cleanup() {
      if (!preserve) {
        std::error_code ignored;
        fs::remove_all(path, ignored);
      }
    }
  } cleanup{workDirectory, false};

  const fs::path manifestPath = workDirectory / "SHA256SUMS.txt";
  const auto manifestDownload = runProcess({
      "curl", "--proto", "=https", "--tlsv1.2", "--fail",
      "--location", "--silent", "--show-error",
      "--output", manifestPath.string(), kManifestUrl}, true);
  if (manifestDownload.exitCode != 0 || !safeDownloadedFile(manifestPath)) {
    printError(
        "OP-UPDATE-003",
        "unable to download the release manifest: " + trim(manifestDownload.output),
        "Check the internet connection and retry later.");
    return 1;
  }

  std::ifstream manifestFile(manifestPath);
  std::ostringstream manifestBuffer;
  manifestBuffer << manifestFile.rdbuf();
  std::string manifestError;
  const auto release =
      parseReleaseManifest(manifestBuffer.str(), manifestError);
  if (!release) {
    printError(
        "OP-UPDATE-004",
        "invalid release manifest: " + manifestError,
        "Do not install the package; retry later or report the release problem.");
    return 1;
  }

  const std::string installed = currentVersion();
  int comparison = 0;
  try {
    comparison = compareVersions(release->version, installed);
  } catch (const std::exception& error) {
    printError("OP-UPDATE-004", error.what(), "Report the version metadata problem.");
    return 1;
  }

  std::cout
      << "OpenPuzzle Update\n"
      << "Installed........... " << installed << '\n'
      << "Latest.............. " << release->version << '\n';

  if (comparison <= 0) {
    std::cout << "Result.............. already up to date\n";
    return 0;
  }
  if (checkOnly) {
    std::cout << "Result.............. update available\n";
    return 0;
  }

  const std::string packageUrl =
      std::string("https://github.com/claudiodb007/OpenPuzzle/") +
      "releases/latest/download/" + release->filename;
  const fs::path packagePath = workDirectory / release->filename;
  const auto packageDownload = runProcess({
      "curl", "--proto", "=https", "--tlsv1.2", "--fail",
      "--location", "--silent", "--show-error",
      "--output", packagePath.string(), packageUrl}, true);
  if (packageDownload.exitCode != 0 || !safeDownloadedFile(packagePath)) {
    printError(
        "OP-UPDATE-003",
        "unable to download the update package: " + trim(packageDownload.output),
        "Check the internet connection and retry later.");
    return 1;
  }

  const auto checksum =
      runProcess({"sha256sum", packagePath.string()}, true);
  std::istringstream checksumFields(checksum.output);
  std::string actualHash;
  checksumFields >> actualHash;
  actualHash = lowercase(actualHash);
  if (checksum.exitCode != 0 || actualHash != release->sha256) {
    printError(
        "OP-UPDATE-004",
        "the downloaded package failed SHA-256 verification",
        "Do not install it; retry later or report the release problem.");
    return 1;
  }

  const auto packageName =
      runProcess({"dpkg-deb", "-f", packagePath.string(), "Package"}, true);
  const auto packageVersion =
      runProcess({"dpkg-deb", "-f", packagePath.string(), "Version"}, true);
  const auto packageArchitecture =
      runProcess({"dpkg-deb", "-f", packagePath.string(), "Architecture"}, true);
  if (packageName.exitCode != 0 || packageVersion.exitCode != 0 ||
      packageArchitecture.exitCode != 0 || trim(packageName.output) != "openpuzzle" ||
      trim(packageVersion.output) != release->version ||
      trim(packageArchitecture.output) != "amd64") {
    printError(
        "OP-UPDATE-004",
        "package metadata does not match the expected OpenPuzzle release",
        "Do not install it; report the release problem.");
    return 1;
  }

  std::cout
      << "SHA-256............ verified\n"
      << "Package............. " << packageName.output
      << "Architecture........ " << trim(packageArchitecture.output) << '\n';

  if (downloadOnly) {
    cleanup.preserve = true;
    std::cout
        << "Result.............. downloaded and verified\n"
        << "Package path........ " << packagePath << '\n';
    return 0;
  }

  std::vector<std::string> installCommand;
  if (geteuid() != 0) {
    if (!commandExists("sudo")) {
      printError(
          "OP-UPDATE-002",
          "sudo is required to install the verified package",
          "Install sudo or run the update as an administrator.");
      return 1;
    }
    installCommand.emplace_back("sudo");
  }
  installCommand.insert(
      installCommand.end(),
      {"apt-get", "install", "--only-upgrade", "--no-remove", "-y",
       packagePath.string()});

  std::cout
      << "Installing........... " << release->version << '\n'
      << "The administrator password may be requested.\n";
  const auto install = runProcess(installCommand, false);
  if (install.exitCode != 0) {
    printError(
        "OP-UPDATE-005",
        "the package manager did not install the update",
        "Read the apt error above, correct it, then rerun 'openpuzzle update'.");
    return 1;
  }

  std::cout
      << "Result.............. updated successfully\n"
      << "Version............. " << release->version << '\n'
      << "Next action......... openpuzzle run\n";
  return 0;
}

} // namespace openpuzzle

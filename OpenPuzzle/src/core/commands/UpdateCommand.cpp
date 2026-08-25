#include "openpuzzle/core/commands/UpdateCommand.hpp"

#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/runtime/ClientRuntimeControl.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <signal.h>
#include <thread>
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

constexpr std::array<const char*, 5> kExecutionSlots = {
    "primary", "gpu", "cpu", "cuda", "opencl"};

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
  for (const char* slot : kExecutionSlots) {
    const auto state =
        client::ClientStateStore::load(slot);

    const auto currentBootId =
        client::ClientStateStore::
            currentBootId();

    if (state &&
        !state->bootId.empty() &&
        !currentBootId.empty() &&
        state->bootId == currentBootId &&
        processExists(state->pid)) {
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

std::vector<std::string> runningRuntimeSlots() {
  std::vector<std::string> slots;
  for (const char* slot : kExecutionSlots) {
    if (ClientRuntimeControl::running(slot)) {
      slots.emplace_back(slot);
    }
  }
  return slots;
}

bool requestSafeStopAndWait(std::string& error) {
  const auto slots = runningRuntimeSlots();
  if (slots.empty()) {
    error = "an execution is active but no controllable runtime was found";
    return false;
  }

  std::vector<std::string> requested;
  for (const auto& slot : slots) {
    if (!ClientRuntimeControl::requestSafeStop(slot)) {
      for (const auto& previous : requested) {
        ClientRuntimeControl::clearSafeStop(previous);
      }
      error = "unable to request safe stop for runtime slot " + slot;
      return false;
    }
    requested.push_back(slot);
  }

  std::cout
      << "Safe stop........... requested\n"
      << "Active ranges....... finishing normally\n"
      << "New assignments..... blocked\n";

  const auto started = std::chrono::steady_clock::now();
  auto nextNotice = started;
  const auto deadline = started + std::chrono::hours(24);

  while (const auto active = activeExecution()) {
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline) {
      error = "safe update waited 24 hours but work is still active (" +
          *active + ")";
      return false;
    }
    if (now >= nextNotice) {
      const auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(
          now - started).count();
      std::cout
          << "Waiting............. current range ("
          << elapsed << " min, " << *active << ")\n";
      nextNotice = now + std::chrono::minutes(1);
    }
    std::this_thread::sleep_for(std::chrono::seconds(5));
  }

  while (!runningRuntimeSlots().empty()) {
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline) {
      error = "safe update waited 24 hours but the runtime is still shutting down";
      return false;
    }
    if (now >= nextNotice) {
      const auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(
          now - started).count();
      std::cout
          << "Waiting............. runtime shutdown ("
          << elapsed << " min)\n";
      nextNotice = now + std::chrono::minutes(1);
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  std::cout << "Runtime............. stopped safely\n";
  return true;
}

bool waitForRuntimeStart() {
  for (int attempt = 0; attempt < 24; ++attempt) {
    if (!runningRuntimeSlots().empty()) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::seconds(5));
  }
  return false;
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

bool startRuntimeDetached(
    const fs::path& logPath,
    std::string& error) {
  if (!privateDirectory(logPath.parent_path())) {
    error = "unable to prepare the runtime log directory";
    return false;
  }

  const pid_t child = fork();
  if (child < 0) {
    error = "unable to fork the OpenPuzzle runtime";
    return false;
  }

  if (child == 0) {
    if (setsid() < 0) {
      _exit(126);
    }
    const int nullDescriptor = open("/dev/null", O_RDONLY);
    const int logDescriptor = open(
        logPath.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0600);
    if (nullDescriptor < 0 || logDescriptor < 0) {
      _exit(126);
    }
    dup2(nullDescriptor, STDIN_FILENO);
    dup2(logDescriptor, STDOUT_FILENO);
    dup2(logDescriptor, STDERR_FILENO);
    close(nullDescriptor);
    close(logDescriptor);
    execlp(
        "openpuzzle", "openpuzzle", "run",
        static_cast<char*>(nullptr));
    _exit(127);
  }

  if (!waitForRuntimeStart()) {
    error = "OpenPuzzle did not report a running runtime within 120 seconds";
    return false;
  }
  return true;
}

bool resumeRuntime(
    const fs::path& updateRoot,
    std::string& error) {
  const fs::path logPath = updateRoot / "openpuzzle-update-resume.log";
  std::cout
      << "Resuming............ openpuzzle run\n"
      << "Runtime log......... " << logPath << '\n';
  return startRuntimeDetached(logPath, error);
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

std::optional<UpdateOptions>
UpdateCommand::parseOptions(
    const std::vector<std::string>& args,
    std::string& error) {
  UpdateOptions options;
  for (const auto& argument : args) {
    if (argument == "--check") {
      options.checkOnly = true;
    } else if (argument == "--download-only") {
      options.downloadOnly = true;
    } else if (argument == "--safe") {
      options.safe = true;
    } else {
      error = "unknown option: " + argument;
      return std::nullopt;
    }
  }

  const int selectedModes =
      static_cast<int>(options.checkOnly) +
      static_cast<int>(options.downloadOnly) +
      static_cast<int>(options.safe);
  if (selectedModes > 1) {
    error = "--check, --download-only and --safe cannot be combined";
    return std::nullopt;
  }
  return options;
}

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
  std::string optionError;
  const auto options = parseOptions(args, optionError);
  if (!options) {
    printError(
        "OP-UPDATE-000",
        optionError,
        "Use: openpuzzle update [--check|--download-only|--safe]");
    return 1;
  }

  if (!options->checkOnly && !options->downloadOnly && !options->safe) {
    if (const auto active = activeExecution()) {
      printError(
          "OP-UPDATE-001",
          "an OpenPuzzle execution is active (" + *active + ")",
          "Run 'openpuzzle safestop', wait for it to finish, then rerun 'openpuzzle update'.");
      return 1;
    }
  }

  std::vector<std::string> requiredCommands = {"curl"};
  if (!options->checkOnly) {
    requiredCommands.emplace_back("sha256sum");
    requiredCommands.emplace_back("dpkg-deb");
    if (!options->downloadOnly) {
      requiredCommands.emplace_back("apt-get");
      if (geteuid() != 0) {
        requiredCommands.emplace_back("sudo");
      }
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
  if (options->checkOnly) {
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

  if (options->downloadOnly) {
    cleanup.preserve = true;
    std::cout
        << "Result.............. downloaded and verified\n"
        << "Package path........ " << packagePath << '\n';
    return 0;
  }

  bool resumeAfterUpdate = false;
  if (options->safe) {
    if (const auto active = activeExecution()) {
      std::cout
          << "Safe update......... active work detected\n"
          << "Execution........... " << *active << '\n';
      std::string safeStopError;
      if (!requestSafeStopAndWait(safeStopError)) {
        printError(
            "OP-UPDATE-006",
            safeStopError,
            "Keep the terminal open, inspect 'openpuzzle status' and retry safely.");
        return 1;
      }
      resumeAfterUpdate = true;
    }
  }

  if (const auto active = activeExecution()) {
    printError(
        "OP-UPDATE-001",
        "an OpenPuzzle execution became active before installation (" +
            *active + ")",
        "Wait for idle state and retry the update.");
    return 1;
  }

  const auto tryResume = [&]() {
    if (!resumeAfterUpdate) {
      return true;
    }
    std::string resumeError;
    if (!resumeRuntime(updateRoot, resumeError)) {
      std::cerr << "Resume warning...... " << resumeError << '\n';
      return false;
    }
    std::cout << "Runtime............. resumed\n";
    return true;
  };

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
    const bool resumed = tryResume();
    printError(
        "OP-UPDATE-005",
        "the package manager did not install the update",
        resumed
            ? "The previous version was resumed; correct the apt error and retry."
            : "Correct the apt error, then run 'openpuzzle run'.");
    return 1;
  }

  const auto installedVersion =
      runProcess({"openpuzzle", "--version"}, true);
  const std::string expectedVersion =
      "OpenPuzzle " + release->version;
  if (installedVersion.exitCode != 0 ||
      trim(installedVersion.output) != expectedVersion) {
    const bool resumed = tryResume();
    printError(
        "OP-UPDATE-005",
        "the installed client did not report " + expectedVersion,
        resumed
            ? "The runtime was resumed; inspect the package installation and retry."
            : "Run 'openpuzzle run' after repairing the package installation.");
    return 1;
  }

  if (!tryResume()) {
    printError(
        "OP-UPDATE-007",
        "the update was installed but OpenPuzzle did not resume",
        "Inspect the runtime log and run 'openpuzzle run'.");
    return 1;
  }

  std::cout
      << "Result.............. updated successfully\n"
      << "Version............. " << release->version << '\n';
  if (resumeAfterUpdate) {
    std::cout << "Next action......... none; OpenPuzzle resumed\n";
  } else {
    std::cout << "Next action......... openpuzzle run\n";
  }
  return 0;
}

} // namespace openpuzzle

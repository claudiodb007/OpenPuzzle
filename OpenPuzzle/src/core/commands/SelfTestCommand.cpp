#include "openpuzzle/core/commands/SelfTestCommand.hpp"

#include "openpuzzle/hardware/RusticlEnvironment.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include "openpuzzle/tools/ToolManager.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace openpuzzle {

namespace {

constexpr const char *Puzzle20Target =
    "1HsMJxNiV7TLxmoF6uJNkydxPFDog4NQum";
constexpr const char *Puzzle20Start = "80000";
constexpr const char *Puzzle20End = "FFFFF";

/*
 * Public, solved puzzle-20 test vector. It is used only
 * for an exact local comparison and is never printed or
 * transmitted by the command.
 */
constexpr const char *Puzzle20CompressedWif =
    "KwDiBf89QgGbjEhKnhXJuH7LrciVrZi3qYjgd9M7rHfuE2Tg4nJW";
constexpr const char *Puzzle20PrivateHex =
    "00000000000000000000000000000000000000000000000000000000000d2c55";

struct Options {
  std::string backend;
  int device = 0;
  int threads = 1;
  std::optional<std::string> rusticlSelector;
  bool deviceSpecified = false;
  bool threadsSpecified = false;
};

struct ProcessOutcome {
  bool started = false;
  int exitCode = -1;
};

std::string lowercase(std::string value) {
  std::transform(
      value.begin(),
      value.end(),
      value.begin(),
      [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
      });

  return value;
}

int positiveInteger(
    const std::string &value,
    const std::string &option,
    bool allowZero) {
  std::size_t consumed = 0;
  long long parsed = 0;

  try {
    parsed = std::stoll(value, &consumed, 10);
  } catch (...) {
    throw std::invalid_argument(option + " requires an integer");
  }

  if (consumed != value.size() ||
      parsed < (allowZero ? 0 : 1) ||
      parsed > 1024) {
    throw std::invalid_argument(option + " has an invalid value");
  }

  return static_cast<int>(parsed);
}

Options parseOptions(const std::vector<std::string> &args) {
  Options options;

  for (std::size_t index = 0; index < args.size(); ++index) {
    const auto &argument = args[index];

    const auto requireValue = [&]() -> const std::string & {
      if (index + 1 >= args.size()) {
        throw std::invalid_argument(argument + " requires a value");
      }

      return args[++index];
    };

    if (argument == "--backend") {
      options.backend = lowercase(requireValue());
    } else if (argument == "--device") {
      options.device = positiveInteger(
          requireValue(), "--device", true);
      options.deviceSpecified = true;
    } else if (argument == "--threads") {
      options.threads = positiveInteger(
          requireValue(), "--threads", false);
      options.threadsSpecified = true;
    } else if (argument == "--rusticl-enable") {
      options.rusticlSelector = requireValue();
    } else {
      throw std::invalid_argument(
          "Unsupported selftest option: " + argument);
    }
  }

  if (options.backend.empty()) {
    throw std::invalid_argument(
        "selftest requires --backend cuda, opencl or cpu");
  }

  if (options.backend != "cuda" &&
      options.backend != "opencl" &&
      options.backend != "cpu") {
    throw std::invalid_argument(
        "Unsupported selftest backend: " + options.backend);
  }

  if (options.backend == "cpu" && options.deviceSpecified) {
    throw std::invalid_argument(
        "--device is not valid for the CPU backend");
  }

  if (options.backend != "cpu" && options.threadsSpecified) {
    throw std::invalid_argument(
        "--threads is only valid for the CPU backend");
  }

  if (options.rusticlSelector && options.backend != "opencl") {
    throw std::invalid_argument(
        "--rusticl-enable requires the OpenCL backend");
  }

  return options;
}

void writePrivateFile(
    const fs::path &path,
    const std::string &content) {
  const int descriptor = ::open(
      path.c_str(),
      O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW,
      S_IRUSR | S_IWUSR);

  if (descriptor < 0) {
    throw std::runtime_error(
        "Unable to create protected self-test file");
  }

  std::size_t offset = 0;

  while (offset < content.size()) {
    const ssize_t written = ::write(
        descriptor,
        content.data() + offset,
        content.size() - offset);

    if (written < 0 && errno == EINTR) {
      continue;
    }

    if (written <= 0) {
      ::close(descriptor);
      throw std::runtime_error(
          "Unable to write protected self-test file");
    }

    offset += static_cast<std::size_t>(written);
  }

  if (::close(descriptor) != 0) {
    throw std::runtime_error(
        "Unable to close protected self-test file");
  }
}

ProcessOutcome runProcess(
    const std::string &executable,
    const std::vector<std::string> &arguments,
    const fs::path &workspace,
    const fs::path &logPath) {
  ProcessOutcome outcome;

  const int logDescriptor = ::open(
      logPath.c_str(),
      O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW,
      S_IRUSR | S_IWUSR);

  if (logDescriptor < 0) {
    throw std::runtime_error(
        "Unable to create protected self-test log");
  }

  const pid_t pid = ::fork();

  if (pid < 0) {
    ::close(logDescriptor);
    throw std::runtime_error(
        "Unable to start the self-test engine");
  }

  if (pid == 0) {
    if (::chdir(workspace.c_str()) != 0 ||
        ::dup2(logDescriptor, STDOUT_FILENO) < 0 ||
        ::dup2(logDescriptor, STDERR_FILENO) < 0) {
      _exit(126);
    }

    if (logDescriptor != STDOUT_FILENO &&
        logDescriptor != STDERR_FILENO) {
      ::close(logDescriptor);
    }

    std::vector<char *> argv;
    argv.reserve(arguments.size() + 2);
    argv.push_back(const_cast<char *>(executable.c_str()));

    for (const auto &argument : arguments) {
      argv.push_back(const_cast<char *>(argument.c_str()));
    }

    argv.push_back(nullptr);
    ::execv(executable.c_str(), argv.data());
    _exit(127);
  }

  outcome.started = true;
  ::close(logDescriptor);

  int status = 0;
  pid_t waited = -1;

  do {
    waited = ::waitpid(pid, &status, 0);
  } while (waited < 0 && errno == EINTR);

  if (waited != pid) {
    outcome.exitCode = -1;
  } else if (WIFEXITED(status)) {
    outcome.exitCode = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    outcome.exitCode = 128 + WTERMSIG(status);
  }

  return outcome;
}

std::string readSmallFile(const fs::path &path) {
  std::error_code error;

  if (!fs::is_regular_file(path, error) || error) {
    return {};
  }

  const auto size = fs::file_size(path, error);

  if (error || size == 0 || size > 4096) {
    return {};
  }

  std::ifstream input(path, std::ios::binary);

  if (!input.is_open()) {
    return {};
  }

  return std::string(
      std::istreambuf_iterator<char>(input),
      std::istreambuf_iterator<char>());
}

bool validPuzzle20Result(const fs::path &path) {
  const std::string content = readSmallFile(path);

  if (content.empty() ||
      content.find(Puzzle20Target) == std::string::npos) {
    return false;
  }

  if (content.find(Puzzle20CompressedWif) != std::string::npos) {
    return true;
  }

  return lowercase(content).find(Puzzle20PrivateHex) !=
         std::string::npos;
}

bool validPuzzle20KeyHuntLog(const fs::path &path) {
  const std::string content = readSmallFile(path);

  if (content.empty() ||
      content.find(Puzzle20Target) == std::string::npos) {
    return false;
  }

  /*
   * KeyHunt reports a successful address-mode hit on
   * standard output instead of writing it to the requested
   * result file. Accept only the exact public puzzle-20
   * private key line together with its expected address.
   */
  const std::regex privateKeyPattern(
      R"(Hit![ \t]+Private Key:[ \t]*0*d2c55[ \t]*(\r?\n|$))",
      std::regex_constants::icase);

  return std::regex_search(
      content,
      privateKeyPattern);
}

double lastSpeed(const fs::path &logPath) {
  std::ifstream input(logPath);

  if (!input.is_open()) {
    return 0.0;
  }

  const std::regex speedPattern(
      R"(([0-9]+(?:\.[0-9]+)?)\s*MKeys?/s)",
      std::regex_constants::icase);

  double speed = 0.0;
  std::string line;

  while (std::getline(input, line)) {
    std::smatch match;

    if (std::regex_search(line, match, speedPattern)) {
      try {
        speed = std::stod(match[1].str());
      } catch (...) {
        /* Ignore malformed performance text. */
      }
    }
  }

  return speed;
}

void preserveFailureLog(
    const fs::path &workspace,
    const fs::path &logPath) {
  std::error_code error;
  const bool hasLog =
      !logPath.empty() &&
      fs::is_regular_file(logPath, error) &&
      !error;

  error.clear();

  if (hasLog) {
    WorkspaceSecurity::protectFile(logPath);
  }

  for (fs::directory_iterator iterator(workspace, error), end;
       iterator != end && !error;
       iterator.increment(error)) {
    const fs::path path = iterator->path();

    if (!hasLog || path != logPath) {
      fs::remove_all(path, error);
      error.clear();
    }
  }

  if (!hasLog) {
    fs::remove_all(workspace, error);
  }
}

} // namespace

int SelfTestCommand::run(
    const std::vector<std::string> &args) const {
  fs::path workspace;
  fs::path logPath;
  fs::path resultPath;

  try {
    const Options options = parseOptions(args);

    if (options.rusticlSelector) {
      RusticlEnvironment::apply(*options.rusticlSelector);
    }

    std::optional<std::string> executable;

    if (options.backend == "cuda") {
      executable = ToolManager::bitcrackCudaPath();
    } else if (options.backend == "opencl") {
      executable = ToolManager::bitcrackOpenCLPath();
    } else {
      executable = ToolManager::keyhuntPath();
    }

    if (!executable ||
        !fs::is_regular_file(*executable) ||
        ::access(executable->c_str(), X_OK) != 0) {
      throw std::runtime_error(
          "The selected self-test engine is unavailable");
    }

    const char *home = std::getenv("HOME");

    if (home == nullptr || *home == '\0') {
      throw std::runtime_error(
          "HOME is unavailable for the self-test workspace");
    }

    const fs::path root =
        fs::path(home) /
        ".local" /
        "share" /
        "OpenPuzzle" /
        "selftest";

    WorkspaceSecurity::prepare(root);

    workspace = root /
        (options.backend + "-" +
         std::to_string(static_cast<long long>(::getpid())));

    std::error_code filesystemError;
    const auto workspaceStatus =
        fs::symlink_status(workspace, filesystemError);

    if ((!filesystemError && fs::exists(workspaceStatus)) ||
        (filesystemError && filesystemError !=
             std::errc::no_such_file_or_directory)) {
      throw std::runtime_error(
          "Unsafe or existing self-test workspace");
    }

    WorkspaceSecurity::prepare(workspace);
    ::umask(S_IRWXG | S_IRWXO);

    logPath = workspace / "engine.log";
    resultPath = workspace / "found.txt";
    std::vector<std::string> engineArguments;

    if (options.backend == "cpu") {
      const fs::path targetsPath = workspace / "targets.txt";
      resultPath = workspace / "KEYFOUNDKEYFOUND.txt";

      writePrivateFile(
          targetsPath,
          std::string(Puzzle20Target) + "\n");
      writePrivateFile(resultPath, "");

      engineArguments = {
          "-m", "address",
          "-f", targetsPath.string(),
          "-r", std::string(Puzzle20Start) + ":" + Puzzle20End,
          "-l", "compress",
          "-n", "1024",
          "-q",
          "-s", "1",
          "-t", std::to_string(options.threads)};
    } else {
      engineArguments = {
          Puzzle20Target,
          "--keyspace",
          std::string(Puzzle20Start) + ":" + Puzzle20End,
          "--out", resultPath.string(),
          "-d", std::to_string(options.device),
          "-b", "16",
          "-t", "128",
          "-p", "256"};
    }

    const auto startedAt = std::chrono::steady_clock::now();
    const ProcessOutcome outcome = runProcess(
        *executable,
        engineArguments,
        workspace,
        logPath);
    const auto finishedAt = std::chrono::steady_clock::now();

    const double duration =
        std::chrono::duration<double>(
            finishedAt - startedAt)
            .count();

    if (fs::is_regular_file(resultPath)) {
      WorkspaceSecurity::protectFile(resultPath);
    }

    WorkspaceSecurity::protectFile(logPath);

    const bool resultMatches =
        validPuzzle20Result(resultPath) ||
        (options.backend == "cpu" &&
         validPuzzle20KeyHuntLog(logPath));

    const bool passed =
        outcome.started &&
        outcome.exitCode == 0 &&
        resultMatches;

    const double speed = lastSpeed(logPath);

    std::cout
        << "OpenPuzzle self-test\n"
        << "--------------------\n"
        << "Backend............ " << options.backend << "\n"
        << "Puzzle............. 20\n"
        << "Result............. " << (passed ? "passed" : "failed") << "\n"
        << "Duration........... " << std::fixed << std::setprecision(3)
        << duration << " seconds\n";

    if (speed > 0.0) {
      std::cout
          << "Speed.............. " << std::fixed << std::setprecision(2)
          << speed << " MKey/s\n";
    } else {
      std::cout << "Speed.............. unavailable\n";
    }

    if (passed) {
      fs::remove_all(workspace, filesystemError);

      if (filesystemError) {
        std::cerr
            << "Self-test passed, but its private temporary files "
               "could not be removed\n";
        return 1;
      }

      return 0;
    }

    preserveFailureLog(workspace, logPath);
    std::cerr << "Diagnostics......... " << workspace.string() << "\n";
    return 1;
  } catch (const std::exception &exception) {
    if (!workspace.empty() && fs::exists(workspace)) {
      try {
        preserveFailureLog(workspace, logPath);
      } catch (...) {
        /* Preserve the original safe error. */
      }

      if (fs::exists(workspace)) {
        std::cerr << "Diagnostics......... " << workspace.string() << "\n";
      }
    }

    std::cerr << "Self-test error..... " << exception.what() << "\n";
    return 1;
  }
}

} // namespace openpuzzle

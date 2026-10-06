#include "openpuzzle/config/ConfigurationManager.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/runtime/ExecutionPersistence.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
std::string read(const std::filesystem::path& path) {
  std::ifstream input(path);
  return {std::istreambuf_iterator<char>(input), {}};
}
void failWrites(const std::function<void()>& action) {
  const pid_t child = fork();
  assert(child >= 0);
  if (child == 0) {
    assert(signal(SIGXFSZ, SIG_IGN) != SIG_ERR);
    const rlimit limit{64, 64};
    assert(setrlimit(RLIMIT_FSIZE, &limit) == 0);
    action();
    _exit(0);
  }
  int status = 0;
  assert(waitpid(child, &status, 0) == child);
  assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}
void assertPrivate(const std::filesystem::path& path) {
  namespace fs = std::filesystem;
  assert(fs::status(path).permissions() ==
      (fs::perms::owner_read | fs::perms::owner_write));
}
}

int main() {
  namespace fs = std::filesystem;
  using namespace openpuzzle;
  const char* previous = std::getenv("HOME");
  const std::string previousHome = previous ? previous : "";
  const fs::path root = fs::temp_directory_path() /
      ("openpuzzle-write-failure-" + std::to_string(getpid()));
  fs::remove_all(root); fs::create_directories(root);
  assert(setenv("HOME", root.c_str(), 1) == 0);
  Configuration configuration;
  configuration.engine.executable = "original executable";
  assert(ConfigurationManager::save(configuration));
  const fs::path configPath = ConfigurationManager::configPath();
  const auto oldConfig = read(configPath);
  assertPrivate(configPath);
  failWrites([&] {
    configuration.engine.executable = std::string(8192, 'x');
    assert(!ConfigurationManager::save(configuration));
  });
  assert(read(configPath) == oldConfig);

  const fs::path workspace = root / "workspace";
  fs::create_directories(workspace);
  ExecutionContext context;
  context.workspace = workspace.string(); context.command = "original command";
  ExecutionPersistence persistence;
  persistence.writeExecutionFile(context);
  ExecutionResult result;
  persistence.writeStateFile(context, "RUNNING", result);
  const auto oldExecution = read(workspace / "execution.json");
  const auto oldState = read(workspace / "state.json");
  assert(!oldExecution.empty() && !oldState.empty());
  assertPrivate(workspace / "execution.json"); assertPrivate(workspace / "state.json");
  failWrites([&] {
    context.command = std::string(8192, 'x');
    persistence.writeExecutionFile(context);
    persistence.writeStateFile(context, "FINISHED", result);
  });
  assert(read(workspace / "execution.json") == oldExecution);
  assert(read(workspace / "state.json") == oldState);

  client::ClientExecutionState state;
  state.active = true; state.assignmentId = "assignment"; state.clientId = "client";
  state.puzzle = 71; state.rangeId = 12; state.pid = 123; state.workspace = workspace.string();
  assert(client::ClientStateStore::save(state, "opencl-1"));
  const auto slotPath = client::ClientStateStore::path("opencl-1");
  const auto oldSlot = read(slotPath);
  failWrites([&] {
    state.command = std::string(8192, 'x');
    assert(!client::ClientStateStore::save(state, "opencl-1"));
  });
  assert(read(slotPath) == oldSlot);
  assertPrivate(slotPath);

  // A failed rename must preserve an existing destination directory and its
  // contents. Temporary files are removed without unlinking the destination.
  const auto blocked = root / "blocked";
  fs::create_directories(blocked / "execution.json");
  { std::ofstream output(blocked / "execution.json" / "keep"); output << "preserve"; }
  context.workspace = blocked.string();
  persistence.writeExecutionFile(context);
  assert(read(blocked / "execution.json" / "keep") == "preserve");
  for (const auto& entry : fs::recursive_directory_iterator(root)) {
    assert(entry.path().filename().string().find(".tmp.") == std::string::npos);
  }
  // A subsequent successful save still publishes a complete replacement.
  assert(ConfigurationManager::save(configuration));
  assert(ConfigurationManager::load().engine.executable == "original executable");
  if (previous) assert(setenv("HOME", previousHome.c_str(), 1) == 0);
  else assert(unsetenv("HOME") == 0);
  fs::remove_all(root);
  std::cout << "PersistenceFailureTests passed\n";
}

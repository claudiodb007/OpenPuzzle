#include "openpuzzle/config/ConfigurationManager.hpp"
#include "openpuzzle/hardware/GpuManager.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

namespace fs = std::filesystem;
using namespace openpuzzle;

namespace {
std::string read(const fs::path& path) {
  std::ifstream input(path);
  return {std::istreambuf_iterator<char>(input), {}};
}

void write(const fs::path& path, const std::string& text) {
  std::ofstream output(path);
  output << text;
  assert(output.good());
}
}

int main() {
  const char* oldHome = std::getenv("HOME");
  const std::string savedHome = oldHome ? oldHome : "";
  const auto root = fs::temp_directory_path() /
      ("openpuzzle-config-read-" + std::to_string(getpid()));
  fs::remove_all(root);
  fs::create_directories(root);
  assert(setenv("HOME", root.c_str(), 1) == 0);
  const fs::path path = ConfigurationManager::configPath();

  // Only an absent file permits creation defaults.
  const auto missing = ConfigurationManager::loadChecked();
  assert(missing && missing->gpu.device == 0 && !fs::exists(path));
  for (const auto& text : {
           std::string{}, std::string(" \n\t"),
           std::string("{\"gpu_device\":7,"), std::string("[]"),
           std::string("null"),
           std::string(R"([{"gpu_device":7,"thermal_enabled":true}])")}) {
    write(path, text);
    assert(!ConfigurationManager::loadChecked());
    assert(ConfigurationManager::load().gpu.device == 0);
    assert(GpuManager::selectedGpu() == 0);
    assert(read(path) == text);
  }

  // The device reader must share the complete-value JSON parser.
  write(path, R"({"gpu_device":2.5})");
  assert(ConfigurationManager::loadChecked());
  assert(GpuManager::selectedGpu() == 0);
  write(path, R"({"gpu_device":"2trailing"})");
  assert(GpuManager::selectedGpu() == 0);
  write(path, R"({"gpu_device":7})");
  assert(GpuManager::selectedGpu() == 7);
  write(path, R"({"gpu":{"gpu_device":3},"engine":{"backend":"opencl"}})");
  const auto nested = ConfigurationManager::loadChecked();
  assert(nested && nested->gpu.device == 3 && nested->engine.backend == "opencl");
  write(path, "{}");
  assert(ConfigurationManager::loadChecked());
  struct stat info {};
  assert(stat(path.c_str(), &info) == 0 && (info.st_mode & 0777) == 0600);

  // A final-path symlink must neither supply settings nor change its target.
  fs::remove(path);
  const auto target = root / "target.json";
  const std::string targetText = R"({"gpu_device":9})";
  write(target, targetText);
  assert(chmod(target.c_str(), 0644) == 0);
  fs::create_symlink(target, path);
  assert(!ConfigurationManager::loadChecked());
  assert(ConfigurationManager::load().gpu.device == 0);
  assert(read(target) == targetText);
  assert(stat(target.c_str(), &info) == 0 && (info.st_mode & 0777) == 0644);
  assert(fs::is_symlink(path));
  fs::remove(path);
  fs::create_symlink(root / "missing-target", path);
  assert(!ConfigurationManager::loadChecked());
  assert(!fs::exists(root / "missing-target"));
  fs::remove(path);
  fs::create_directory(path);
  assert(!ConfigurationManager::loadChecked());
  assert(ConfigurationManager::load().gpu.device == 0);
  assert(fs::is_directory(path));
  fs::remove(path);

  // Bound a child explicitly so a blocking FIFO regression cannot hang CTest.
  assert(mkfifo(path.c_str(), 0600) == 0);
  const pid_t child = fork();
  assert(child >= 0);
  if (child == 0) {
    const bool rejected = !ConfigurationManager::loadChecked();
    const bool defaults = ConfigurationManager::load().gpu.device == 0;
    _exit(rejected && defaults ? 0 : 1);
  }
  int status = 0;
  pid_t completed = 0;
  for (int attempt = 0; attempt < 100 && completed == 0; ++attempt) {
    completed = waitpid(child, &status, WNOHANG);
    if (completed == 0) usleep(10000);
  }
  if (completed == 0) {
    kill(child, SIGKILL);
    waitpid(child, &status, 0);
  }
  assert(completed == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
  assert(fs::is_fifo(path));

  if (oldHome) assert(setenv("HOME", savedHome.c_str(), 1) == 0);
  else assert(unsetenv("HOME") == 0);
  fs::remove_all(root);
}

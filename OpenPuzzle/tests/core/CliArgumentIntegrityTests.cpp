#include "openpuzzle/config/ConfigurationManager.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"
#include "openpuzzle/core/Application.hpp"
#include "openpuzzle/runtime/RunSession.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

using namespace openpuzzle;
namespace fs = std::filesystem;

namespace {
struct Environment {
  std::string name, original;
  bool present;
  Environment(const char* key, const std::string& value) : name(key) {
    const char* prior = std::getenv(key);
    present = prior != nullptr;
    if (prior) original = prior;
    assert(setenv(key, value.c_str(), 1) == 0);
  }
  ~Environment() {
    if (present) setenv(name.c_str(), original.c_str(), 1);
    else unsetenv(name.c_str());
  }
};
struct Output {
  std::ostringstream text;
  std::streambuf* out = std::cout.rdbuf(text.rdbuf());
  std::streambuf* err = std::cerr.rdbuf(text.rdbuf());
  ~Output() { std::cout.rdbuf(out); std::cerr.rdbuf(err); }
};
std::string read(const fs::path& path) {
  std::ifstream input(path);
  return {std::istreambuf_iterator<char>(input), {}};
}
int application(std::vector<std::string> args) {
  args.insert(args.begin(), "openpuzzle");
  std::vector<char*> argv;
  for (auto& arg : args) argv.push_back(arg.data());
  return Application{}.run(static_cast<int>(argv.size()), argv.data());
}
}

int main() {
  const auto home = fs::temp_directory_path() /
      ("openpuzzle-cli-integrity-" + std::to_string(getpid()));
  fs::remove_all(home);
  fs::create_directories(home / ".local/share/OpenPuzzle");
  Environment temporaryHome("HOME", home.string());
  Environment rusticl("RUSTICL_ENABLE", "original-selector");
  Environment server("OPENPUZZLE_SERVER_URL", "http://127.0.0.1:1");
  Configuration original;
  original.gpu.device = 7;
  original.gpu.rusticlEnable = "radeonsi";
  original.gpu.thermal.enabled = true;
  original.assignment.durationMinutes = 42;
  assert(ConfigurationManager::save(original));
  const fs::path config = ConfigurationManager::configPath();
  const auto bytes = read(config);
  const auto database = home / ".local/share/OpenPuzzle/openpuzzle.db";
  Environment slot("OPENPUZZLE_EXECUTION_SLOT", "primary");
  client::ClientExecutionState previous;
  previous.active = true;
  previous.assignmentId = "preserved-previous-assignment";
  previous.clientId = "isolated-test-client";
  previous.puzzle = 71;
  previous.rangeId = 1;
  previous.pid = 2147483647;
  previous.workspace = (home / "preserve-me").string();
  assert(client::ClientStateStore::save(previous));
  const auto state = client::ClientStateStore::path();
  const auto stateBytes = read(state);
  assert(client::ClientStateStore::load());

  const auto unchanged = [&] {
    assert(read(config) == bytes);
    assert(read(state) == stateBytes);
    assert(std::string(getenv("RUSTICL_ENABLE")) == "original-selector");
    assert(!fs::exists(database));
    assert(!fs::exists(home / ".config/OpenPuzzle/client.id"));
  };
  for (const auto& request : std::vector<std::vector<std::string>>{
           {"--device", "2.5"}, {"--device", "2junk"}, {"--device"},
           {"--device", ""}, {"--device", "2147483648"}, {"--device", "-1"},
           {"--device", "2", "--device", "3"}}) {
    auto args = request; args.insert(args.begin(), "gpu-select");
    Output output;
    assert(application(args) != 0);
    assert(output.text.str().find("Selected GPU device") == std::string::npos);
    unchanged();
  }

  for (const auto& request : std::vector<std::vector<std::string>>{
           {"--gpu", "2.5"}, {"--d", "2text"}, {"--gpu"},
           {"--seconds", "30.5"}, {"--seconds", "30text"}, {"--seconds"},
           {"--samples", "8text"}, {"--samples", ""}, {"--samples"},
           {"--blocks", "256.5"}, {"--threads", "256text"}, {"--points"},
           {"--seconds", "30", "--seconds", "40"},
           {"--blocks", "256", "--b", "128"},
           {"--gpu", "0", "--d", "1"}}) {
    std::vector<std::string> args = {
        "benchmark", "--backend", "opencl", "--rusticl-enable", "iris"};
    args.insert(args.end(), request.begin(), request.end());
    Output output;
    assert(application(args) != 0);
    assert(output.text.str().find("OP-BENCH-011") != std::string::npos);
    unchanged();
  }

  for (const auto& request : std::vector<std::vector<std::string>>{
           {"--blocks", "bad"}, {"--blocks", "0"}, {"--blocks"},
           {"--threads", "256.5"}, {"--points", "-1"},
           {"--device", "0", "--d", "1"}, {"--puzzle", "71text"},
           {"--job", "2147483648"}}) {
    auto args = request; args.insert(args.begin(), "start-job");
    args.push_back("--dry-run");
    Output output;
    assert(application(args) != 0);
    assert(output.text.str().find("Could not open database") == std::string::npos);
    unchanged();
  }

  // Invalid requests must stop before recovery,
  // supervisor discovery, local context setup, Rusticl changes or HTTP work.
  for (const auto& request : std::vector<std::vector<std::string>>{
           {"--blocks", "0"}, {"--blocks", "256.5"}, {"--blocks"},
           {"--threads", "256text"}, {"--points", "2147483648"},
           {"--device", ""}, {"--device"}, {"--device", "2.5"},
           {"--duration-minutes"}, {"--duration-minutes", "0"},
           {"--duration-minutes", "361"},
           {"--puzzle", "71text"}, {"--puzzle", "0"},
           {"--points", "256", "--p", "128"},
           {"--opencl-device"}, {"--cpu-threads", "2.5"}}) {
    for (const auto& mode : std::vector<std::vector<std::string>>{
             {}, {"--devices", "all"}, {"--with-opencl"}, {"--with-cpu"}}) {
      std::vector<std::string> args = {
          "run", "71", "--dry-run", "--engine", "bitcrack",
          "--backend", "opencl", "--rusticl-enable", "iris"};
      args.insert(args.end(), mode.begin(), mode.end());
      args.insert(args.end(), request.begin(), request.end());
      Output output;
      RunSession session;
      assert(session.run(args) != 0);
      assert(output.text.str().find("argument validation failed") != std::string::npos);
      assert(output.text.str().find("Recovering") == std::string::npos);
      unchanged();
    }
  }

  // Valid whole-number device selection remains usable without a GPU search.
  assert(application({"gpu-select", "--device", "+2"}) == 0);
  assert(ConfigurationManager::load().gpu.device == 2);
  fs::remove_all(home);
}

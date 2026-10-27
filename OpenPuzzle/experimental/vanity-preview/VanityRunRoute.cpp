// Local RunSession integration. No production endpoint or ordinary claim fallback.
#include "VanityRunRoute.hpp"
#include "VanityCommand.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <unistd.h>

namespace openpuzzle::vanityPreview {
namespace {
void need(bool ok, const char* message) {if (!ok) throw std::runtime_error(message);}
std::string lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {return std::tolower(c);});
  return value;
}
}
int runLocalSessionRoute(const std::vector<std::string>& args) {
  const bool local = std::find(args.begin(), args.end(), "--vanity-session") != args.end()
      || std::find(args.begin(), args.end(), "--vanity-native-local") != args.end();
  if (!local) {
    std::cerr << "Vanity is experimental: production assignments are not enabled.\n"
              << "No assignment requested.\n";
    return 1;
  }
  try {
    need(args.size() >= 2 && args[0] == "run" && args[1] == "20", "Explicit local run 20 required");
    const std::set<std::string> flags = {"--vanity-native-local", "--enable-experimental-vanity-local", "--resume"};
    std::map<std::string, std::string> values;
    std::set<std::string> enabled;
    for (std::size_t i = 2; i < args.size(); ++i) {
      const auto& key = args[i];
      need(!key.empty() && key.starts_with("--"), "Unexpected local positional argument");
      if (flags.count(key)) {need(enabled.insert(key).second, "Repeated local flag"); continue;}
      need(i + 1 < args.size(), "Missing local option value");
      const auto& value = args[++i];
      need(!value.empty() && value.find_first_of("\r\n\0", 0, 3) == std::string::npos
          && values.emplace(key, value).second, "Invalid/repeated local option");
    }
    need(values.count("--engine") && lower(values.at("--engine")) == "vanity"
        && values.count("--backend") && lower(values.at("--backend")) == "cuda", "Local Vanity/CUDA selection required");
    values.erase("--engine"); values.erase("--backend");
    if (enabled.count("--vanity-native-local")) {
      need(!values.count("--vanity-session"), "Mixed local session routes");
      const std::set<std::string> fields = {"--server", "--root", "--controls", "--target-engine", "--engine-sha256", "--adapter", "--adapter-sha256", "--client-id", "--target", "--grid", "--segment-keys", "--scope-start", "--scope-end", "--gpu-uuid", "--speed-mkeys"};
      need(values.size() == fields.size() && enabled.count("--enable-experimental-vanity-local"), "Explicit native local binding required");
      for (const auto& [key, value] : values) need(fields.count(key), "Unsupported local native option");
      std::vector<std::string> native = {"coordinator-test", "--enable-experimental-vanity-local", "--puzzle", "20"};
      for (const auto& [key, value] : values) {native.push_back(key == "--target-engine" ? "--engine" : key); native.push_back(value);}
      if (enabled.count("--resume")) native.push_back("--resume");
      return runVanityCommand(native);
    }
    need(!enabled.count("--resume") && values.count("--vanity-session"), "Explicit desktop session action required");
    const auto action = values.at("--vanity-session");
    need(action == "start" || action == "resume" || action == "runs", "Unknown local session action");
    const std::set<std::string> fields = action == "runs" ? std::set<std::string>{"--vanity-session", "--store"}
        : action == "start" ? std::set<std::string>{"--vanity-session", "--store", "--run-root"}
        : std::set<std::string>{"--vanity-session", "--store", "--run-root", "--expected-state"};
    need(values.size() == fields.size(), "Missing/extra desktop binding options");
    for (const auto& [key, value] : values) need(fields.count(key), "Unsupported local desktop option");
    need(action == "runs" || enabled.count("--enable-experimental-vanity-local"), "Explicit desktop opt-in required");
    std::vector<std::string> command = {OPENPUZZLE_VANITY_PYTHON, OPENPUZZLE_VANITY_COORDINATOR_SCRIPT,
      action, "--cli", std::filesystem::canonical("/proc/self/exe").string()};
    for (const auto& [key, value] : values) {if (key != "--vanity-session") {command.push_back(key); command.push_back(value);}}
    if (enabled.count("--enable-experimental-vanity-local")) command.push_back("--enable-experimental-vanity-local");
    std::vector<char*> argv; for (auto& value : command) argv.push_back(value.data()); argv.push_back(nullptr);
    execv(argv[0], argv.data());
    throw std::runtime_error("Local coordinator desktop helper could not start");
  } catch (const std::exception& error) {
    std::cerr << "Local Vanity RunSession refused: " << error.what() << '\n'; return 2;
  }
}
}

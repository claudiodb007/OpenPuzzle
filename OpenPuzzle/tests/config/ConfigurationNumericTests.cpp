#include "openpuzzle/config/ConfigurationManager.hpp"
#include "openpuzzle/runtime/ExecutionPersistence.hpp"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>
#include <string>
#include <unistd.h>

namespace {
struct CommaNumbers : std::numpunct<char> {
  char do_decimal_point() const override { return ','; }
  char do_thousands_sep() const override { return '.'; }
  std::string do_grouping() const override { return "\3"; }
};
std::string read(const std::filesystem::path& path) {
  std::ifstream input(path);
  return {std::istreambuf_iterator<char>(input), {}};
}
}

int main() {
  namespace fs = std::filesystem;
  using namespace openpuzzle;
  const char* previous = std::getenv("HOME");
  const std::string previousHome = previous ? previous : "";
  const fs::path root = fs::temp_directory_path() /
      ("openpuzzle-config-numeric-" + std::to_string(getpid()));
  fs::remove_all(root);
  fs::create_directories(root);
  assert(setenv("HOME", root.c_str(), 1) == 0);
  Configuration configuration;
  assert(ConfigurationManager::save(configuration));

  // An incomplete document must not partially enable protection or change GPUs.
  const std::string damaged =
      R"({"gpu_device":2,"thermal_enabled":true,"thermal_warning_c":70.5,"duration_minutes":3)";
  { std::ofstream output(ConfigurationManager::configPath()); output << damaged; }
  const auto defaults = ConfigurationManager::load();
  assert(defaults.gpu.device == 0 && !defaults.gpu.thermal.enabled);
  assert(defaults.gpu.thermal.warningC == 75.0);
  assert(defaults.assignment.durationMinutes == 60);
  assert(read(ConfigurationManager::configPath()) == damaged);

  // Numeric fields must consume the complete value, not a decimal prefix.
  {
    std::ofstream output(ConfigurationManager::configPath());
    output << R"({"gpu_device":2.5,"duration_minutes":"7days","thermal_enabled":"trueX","thermal_warning_c":"73.5C","thermal_critical_c":"NaN"})";
  }
  const auto invalid = ConfigurationManager::load();
  assert(invalid.gpu.device == 0 && invalid.assignment.durationMinutes == 60);
  assert(!invalid.gpu.thermal.enabled);
  assert(invalid.gpu.thermal.warningC == 75.0 && invalid.gpu.thermal.criticalC == 85.0);

  const std::locale previousLocale = std::locale();
  std::locale::global(std::locale(std::locale::classic(), new CommaNumbers));
  configuration.engine.executable = "/tmp/quoted \"engine\"";
  configuration.gpu.device = 1234;
  configuration.gpu.thermal.enabled = true;
  configuration.gpu.thermal.warningC = 73.12345678901234;
  configuration.gpu.thermal.criticalC = 85.5;
  configuration.assignment.durationMinutes = 1234;
  assert(ConfigurationManager::save(configuration));
  boost::property_tree::ptree json;
  boost::property_tree::read_json(ConfigurationManager::configPath(), json);
  assert(json.get<int>("gpu_device") == 1234);
  const auto loaded = ConfigurationManager::load();
  assert(loaded.gpu.device == 1234 && loaded.assignment.durationMinutes == 1234);
  assert(loaded.gpu.thermal.enabled);
  assert(loaded.gpu.thermal.warningC == configuration.gpu.thermal.warningC);
  assert(loaded.engine.executable == configuration.engine.executable);

  ExecutionContext context;
  context.workspace = (root / "workspace").string();
  context.executionId = 1234;
  fs::create_directories(context.workspace);
  ExecutionPersistence persistence;
  persistence.writeExecutionFile(context);
  boost::property_tree::read_json((fs::path(context.workspace) / "execution.json").string(), json);
  assert(json.get<int>("execution_id") == 1234);
  ExecutionResult result;
  result.linesRead = 1234;
  result.averageSpeed = 12345.123456789012;
  persistence.writeStateFile(context, "RUNNING", result);
  boost::property_tree::read_json((fs::path(context.workspace) / "state.json").string(), json);
  assert(json.get<std::uint64_t>("lines_read") == 1234);
  // Boost's generic conversion itself inherits the global locale: read the
  // JSON token with an explicitly classic stream for the numeric comparison.
  std::istringstream speed(json.get<std::string>("average_speed"));
  speed.imbue(std::locale::classic());
  double restoredSpeed = 0.0;
  speed >> restoredSpeed;
  assert(restoredSpeed == result.averageSpeed);
  std::locale::global(previousLocale);

  const std::string saved = read(ConfigurationManager::configPath());
  configuration.gpu.thermal.warningC = std::numeric_limits<double>::infinity();
  assert(!ConfigurationManager::save(configuration));
  assert(read(ConfigurationManager::configPath()) == saved);
  const fs::path statePath = fs::path(context.workspace) / "state.json";
  const auto savedState = read(statePath);
  result.averageSpeed = std::numeric_limits<double>::quiet_NaN();
  persistence.writeStateFile(context, "FINISHED", result);
  assert(read(statePath) == savedState);

  if (previous) assert(setenv("HOME", previousHome.c_str(), 1) == 0);
  else assert(unsetenv("HOME") == 0);
  fs::remove_all(root);
  std::cout << "ConfigurationNumericTests passed\n";
}

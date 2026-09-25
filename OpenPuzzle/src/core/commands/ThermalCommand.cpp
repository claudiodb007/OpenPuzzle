#include "openpuzzle/core/commands/ThermalCommand.hpp"

#include "openpuzzle/config/ConfigurationManager.hpp"
#include "openpuzzle/hardware/GpuThermalPolicy.hpp"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>

namespace openpuzzle {

namespace {

struct ThermalCommandOptions {
  bool enable = false;
  bool disable = false;
  bool stopOnCritical = false;
  bool diagnosticOnly = false;
  bool help = false;
  std::optional<double> warningC;
  std::optional<double> criticalC;
};

void printUsage() {
  std::cout
      << "OpenPuzzle Thermal Policy\n"
      << "-------------------------\n"
      << "Usage:\n"
      << "  openpuzzle thermal\n"
      << "  openpuzzle thermal --enable [--warning-c N] [--critical-c N]\n"
      << "  openpuzzle thermal --disable\n"
      << "  openpuzzle thermal --stop-on-critical|--diagnostic-only\n"
      << "  openpuzzle thermal --warning-c N --critical-c N\n"
      << "\n"
      << "Critical stopping is opt-in and orderly. OpenPuzzle never changes\n"
      << "GPU power, clocks or fans.\n";
}

std::optional<double> parseTemperature(const std::string &value) {
  try {
    std::size_t consumed = 0;
    const double temperature = std::stod(value, &consumed);

    if (consumed != value.size() || !std::isfinite(temperature)) {
      return std::nullopt;
    }

    return temperature;
  } catch (...) {
    return std::nullopt;
  }
}

bool parseOptions(
    const std::vector<std::string> &args,
    ThermalCommandOptions &options,
    std::string &error) {
  bool warningSeen = false;
  bool criticalSeen = false;

  for (std::size_t index = 0; index < args.size(); ++index) {
    const auto &argument = args[index];

    if (argument == "--help" || argument == "-h") {
      options.help = true;
    } else if (argument == "--enable") {
      if (options.enable) {
        error = "--enable was provided more than once";
        return false;
      }
      options.enable = true;
    } else if (argument == "--disable") {
      if (options.disable) {
        error = "--disable was provided more than once";
        return false;
      }
      options.disable = true;
    } else if (argument == "--stop-on-critical") {
      if (options.stopOnCritical) {
        error = "--stop-on-critical was provided more than once";
        return false;
      }
      options.stopOnCritical = true;
    } else if (argument == "--diagnostic-only") {
      if (options.diagnosticOnly) {
        error = "--diagnostic-only was provided more than once";
        return false;
      }
      options.diagnosticOnly = true;
    } else if (argument == "--warning-c" ||
               argument == "--critical-c") {
      const bool warning = argument == "--warning-c";
      bool &seen = warning ? warningSeen : criticalSeen;

      if (seen) {
        error = argument + " was provided more than once";
        return false;
      }

      if (index + 1 >= args.size()) {
        error = argument + " requires a temperature";
        return false;
      }

      const auto value = parseTemperature(args[++index]);
      if (!value) {
        error = argument + " requires a finite number";
        return false;
      }

      if (warning) {
        options.warningC = value;
      } else {
        options.criticalC = value;
      }
      seen = true;
    } else {
      error = "unknown thermal option: " + argument;
      return false;
    }
  }

  if (options.enable && options.disable) {
    error = "--enable and --disable cannot be used together";
    return false;
  }

  if (options.stopOnCritical && options.diagnosticOnly) {
    error = "--stop-on-critical and --diagnostic-only cannot be used together";
    return false;
  }

  return true;
}

void printPolicy(
    const GpuThermalPolicyConfiguration &policy,
    const bool updated) {
  std::cout << "OpenPuzzle Thermal Policy\n"
            << "-------------------------\n"
            << "Mode................ "
            << (policy.enabled ? "enabled" : "disabled") << '\n'
            << std::fixed << std::setprecision(1)
            << "Warning threshold... " << policy.warningC << " C\n"
            << "Critical threshold.. " << policy.criticalC << " C\n"
            << "Enforcement......... "
            << (policy.stopOnCritical
                    ? "orderly stop at critical threshold"
                    : "none; diagnostic warnings only")
            << '\n'
            << "Configuration....... "
            << (updated ? "updated" : "unchanged") << '\n';
}

} // namespace

int ThermalCommand::run(
    const std::vector<std::string> &args) const {
  ThermalCommandOptions options;
  std::string error;

  if (!parseOptions(args, options, error)) {
    std::cerr << "OpenPuzzle thermal configuration failed\n"
              << "---------------------------------------\n"
              << "Problem............ " << error << '\n'
              << "Configuration...... unchanged\n";
    return 1;
  }

  if (options.help) {
    printUsage();
    return 0;
  }

  auto configuration = ConfigurationManager::load();
  auto policy = configuration.gpu.thermal;
  const auto previous = policy;

  if (options.enable) {
    policy.enabled = true;
  } else if (options.disable) {
    policy.enabled = false;
  }

  if (options.stopOnCritical) {
    policy.stopOnCritical = true;
  } else if (options.diagnosticOnly) {
    policy.stopOnCritical = false;
  }

  if (options.warningC) {
    policy.warningC = *options.warningC;
  }

  if (options.criticalC) {
    policy.criticalC = *options.criticalC;
  }

  if (!GpuThermalPolicy::valid(policy)) {
    std::cerr
        << "OpenPuzzle thermal configuration failed\n"
        << "---------------------------------------\n"
        << "Problem............ invalid thermal thresholds\n"
        << "Warning range...... 30.0 to 110.0 C\n"
        << "Critical range..... above warning, up to 120.0 C\n"
        << "Configuration...... unchanged\n";
    return 1;
  }

  const bool changed =
      policy.enabled != previous.enabled ||
      policy.stopOnCritical != previous.stopOnCritical ||
      policy.warningC != previous.warningC ||
      policy.criticalC != previous.criticalC;

  if (changed) {
    configuration.gpu.thermal = policy;

    if (!ConfigurationManager::save(configuration)) {
      std::cerr
          << "OpenPuzzle thermal configuration failed\n"
          << "---------------------------------------\n"
          << "Problem............ unable to save configuration\n"
          << "Configuration...... unchanged\n";
      return 1;
    }
  }

  printPolicy(policy, changed);
  return 0;
}

} // namespace openpuzzle

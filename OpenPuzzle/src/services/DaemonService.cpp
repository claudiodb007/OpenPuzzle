#include "openpuzzle/services/DaemonService.hpp"

#include "openpuzzle/runtime/DaemonRunner.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace openpuzzle {

int DaemonService::execute(
    const std::vector<std::string>& args) {
  int ticks = 3;
  int syncIntervalSeconds = 60;

  std::string serverUrl =
      "https://claudiodb.com";

  if (const char* environment =
          std::getenv(
              "OPENPUZZLE_SERVER_URL")) {
    if (*environment != '\0') {
      serverUrl = environment;
    }
  }

  for (std::size_t index = 0;
       index < args.size();
       ++index) {
    const auto& argument =
        args[index];

    if (argument == "--ticks" ||
        argument == "--server" ||
        argument == "--sync-interval") {
      if (index + 1 >= args.size()) {
        std::cerr
            << "Missing value for "
            << argument
            << '\n';

        return 1;
      }

      const auto& value =
          args[++index];

      try {
        if (argument == "--ticks") {
          ticks =
              std::stoi(value);

          if (ticks < 0) {
            throw std::invalid_argument(
                "negative ticks");
          }
        } else if (
            argument ==
            "--sync-interval") {
          syncIntervalSeconds =
              std::stoi(value);

          if (syncIntervalSeconds < 0) {
            throw std::invalid_argument(
                "negative sync interval");
          }
        } else {
          serverUrl = value;

          if (serverUrl.empty()) {
            throw std::invalid_argument(
                "empty server URL");
          }
        }
      } catch (...) {
        std::cerr
            << "Invalid value for "
            << argument
            << ": "
            << value
            << '\n';

        return 1;
      }

      continue;
    }

    std::cerr
        << "Unknown daemon option: "
        << argument
        << '\n';

    return 1;
  }

  std::cout
      << "Server............ "
      << serverUrl
      << '\n'
      << "Sync interval..... "
      << syncIntervalSeconds
      << " seconds\n";

  DaemonRunner runner(
      database_,
      serverUrl,
      syncIntervalSeconds);

  return runner.run(ticks);
}

} // namespace openpuzzle

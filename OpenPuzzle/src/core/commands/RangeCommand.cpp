#include "openpuzzle/core/commands/RangeCommand.hpp"

#include "openpuzzle/client/ClientIdentity.hpp"
#include "openpuzzle/client/HttpRangeClient.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace openpuzzle {

namespace {

std::string getArgument(
    const std::vector<std::string>& args,
    const std::string& name,
    const std::string& fallback = {}) {
  for (std::size_t index = 0;
       index + 1 < args.size();
       ++index) {
    if (args[index] == name) {
      return args[index + 1];
    }
  }

  return fallback;
}

int getIntegerArgument(
    const std::vector<std::string>& args,
    const std::string& name,
    int fallback) {
  const auto value =
      getArgument(
          args,
          name);

  if (value.empty()) {
    return fallback;
  }

  return std::stoi(value);
}

} // namespace

int RangeCommand::run(
    const std::vector<std::string>& args) const {
  if (args.empty() ||
      args.front() != "claim") {
    std::cerr
        << "Usage: OpenPuzzle range claim "
        << "--server <url> "
        << "[--puzzle <number>]\n";

    return 1;
  }

  std::string defaultServer;

  if (const char* environment =
          std::getenv(
              "OPENPUZZLE_SERVER_URL")) {
    defaultServer =
        environment;
  }

  const std::string serverUrl =
      getArgument(
          args,
          "--server",
          defaultServer);

  if (serverUrl.empty()) {
    std::cerr
        << "Server URL is required.\n"
        << "Use --server <url> or set "
        << "OPENPUZZLE_SERVER_URL.\n";

    return 1;
  }

  const int puzzle =
      getIntegerArgument(
          args,
          "--puzzle",
          71);

  const std::string clientId =
      client::ClientIdentity::loadOrCreate();

  if (clientId.empty()) {
    std::cerr
        << "Unable to create local client identity\n";

    return 1;
  }

  std::cout
      << "OpenPuzzle Range Client\n"
      << "-----------------------\n"
      << "Server............. "
      << serverUrl
      << '\n'
      << "Client ID.......... "
      << clientId
      << '\n'
      << "Puzzle............. "
      << puzzle
      << '\n'
      << "Requesting range...\n\n";

  client::HttpRangeClient client(
      serverUrl);

  const auto assignment =
      client.claim(
          clientId,
          puzzle);

  if (!assignment) {
    std::cerr
        << "Unable to claim range: "
        << client.lastError()
        << '\n';

    return 1;
  }

  std::cout
      << "Assignment......... "
      << assignment->assignmentId
      << '\n'
      << "Puzzle............. "
      << assignment->puzzle
      << '\n'
      << "Range ID........... "
      << assignment->rangeId
      << '\n'
      << "Target............. "
      << assignment->target
      << '\n'
      << "Start.............. "
      << assignment->start
      << '\n'
      << "End................ "
      << assignment->end
      << '\n';

  return 0;
}

} // namespace openpuzzle

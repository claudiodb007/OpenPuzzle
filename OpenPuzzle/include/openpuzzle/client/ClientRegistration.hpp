#pragma once

#include <string>

namespace openpuzzle::client {

struct ClientRegistration {
  std::string clientId;
  std::string hostname;
  std::string platform;
  std::string version;

  bool valid() const {
    return !clientId.empty() &&
           !hostname.empty() &&
           !platform.empty() &&
           !version.empty();
  }
};

} // namespace openpuzzle::client

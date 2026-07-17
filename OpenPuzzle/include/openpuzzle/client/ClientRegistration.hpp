#pragma once

#include <string>

namespace openpuzzle::client {

struct ClientRegistration {
  std::string clientId;

  bool valid() const {
    return !clientId.empty();
  }
};

} // namespace openpuzzle::client

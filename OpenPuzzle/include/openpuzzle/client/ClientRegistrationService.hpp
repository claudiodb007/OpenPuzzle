#pragma once

#include "openpuzzle/client/ClientRegistration.hpp"

#include <string>

namespace openpuzzle::client {

struct ClientRegistrationResult {
  bool success = false;

  ClientRegistration registration;

  std::string error;
};

class ClientRegistrationService {
public:
  ClientRegistrationResult registerWith(
      const std::string& serverUrl) const;

  static ClientRegistration
  collectLocalRegistration();
};

} // namespace openpuzzle::client

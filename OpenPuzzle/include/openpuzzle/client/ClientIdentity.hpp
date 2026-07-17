#pragma once

#include <string>

namespace openpuzzle::client {

class ClientIdentity {
public:
  static std::string loadOrCreate();
};

} // namespace openpuzzle::client

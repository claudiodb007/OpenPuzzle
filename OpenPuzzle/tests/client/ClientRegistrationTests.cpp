#include "openpuzzle/client/ClientRegistration.hpp"

#include <cassert>
#include <iostream>

using namespace openpuzzle::client;

int main() {
  ClientRegistration registration;

  assert(!registration.valid());

  registration.clientId =
      "11111111-1111-4111-8111-111111111111";

  registration.hostname =
      "worker-01";

  registration.platform =
      "Ubuntu Linux";

  registration.version =
      "0.10.0";

  assert(registration.valid());

  registration.hostname.clear();

  assert(!registration.valid());

  std::cout
      << "ClientRegistrationTests passed\n";

  return 0;
}

#include "openpuzzle/client/ClientRegistrationService.hpp"

#include "openpuzzle/client/ClientIdentity.hpp"
#include "openpuzzle/client/HttpRangeClient.hpp"

namespace openpuzzle::client {

ClientRegistration
ClientRegistrationService::
collectLocalRegistration() {
  ClientRegistration registration;

  registration.clientId =
      ClientIdentity::loadOrCreate();

  return registration;
}

ClientRegistrationResult
ClientRegistrationService::registerWith(
    const std::string& serverUrl) const {
  ClientRegistrationResult result;

  result.registration =
      collectLocalRegistration();

  if (!result.registration.valid()) {
    result.error =
        "Unable to collect valid client registration";

    return result;
  }

  HttpRangeClient httpClient(
      serverUrl);

  result.success =
      httpClient.registerClient(
          result.registration);

  if (!result.success) {
    result.error =
        httpClient.lastError();
  }

  return result;
}

} // namespace openpuzzle::client

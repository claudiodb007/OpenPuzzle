#pragma once

#include "openpuzzle/client/ClientHeartbeat.hpp"
#include "openpuzzle/client/ClientRegistration.hpp"
#include "openpuzzle/client/RangeAssignment.hpp"

#include <optional>
#include <string>

namespace openpuzzle::client {

class HttpRangeClient {
public:
  explicit HttpRangeClient(std::string serverUrl);

  bool registerClient(const ClientRegistration &registration);

  bool heartbeat(const ClientHeartbeat &heartbeat);

  std::optional<RangeAssignment> claim(const std::string &clientId, int puzzle,
                                       int targetDurationMinutes,
                                       double speedMKeys = 0.0);

  bool complete(const std::string &assignmentId, const std::string &clientId,
                int exitCode, const std::string &status = "completed");

  bool progress(const std::string &assignmentId, const std::string &clientId,
                double speedMKeys, const std::string &keysChecked);

  static std::optional<RangeAssignment>
  parseClaimResponse(const std::string &response, std::string &error);

  const std::string &lastError() const;

private:
  std::string serverUrl_;
  std::string lastError_;

  static std::string shellQuote(const std::string &value);

  static std::string normalizeServerUrl(std::string value);
};

} // namespace openpuzzle::client

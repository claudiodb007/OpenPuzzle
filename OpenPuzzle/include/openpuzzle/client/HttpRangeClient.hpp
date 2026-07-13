#pragma once

#include "openpuzzle/client/RangeAssignment.hpp"

#include <optional>
#include <string>

namespace openpuzzle::client {

class HttpRangeClient {
public:
  explicit HttpRangeClient(
      std::string serverUrl);

  std::optional<RangeAssignment> claim(
      const std::string& clientId,
      int puzzle);

  bool complete(
      const std::string& assignmentId,
      const std::string& clientId,
      int exitCode);

  static std::optional<RangeAssignment>
  parseClaimResponse(
      const std::string& response,
      std::string& error);

  const std::string& lastError() const;

private:
  std::string serverUrl_;
  std::string lastError_;

  static std::string shellQuote(
      const std::string& value);

  static std::string normalizeServerUrl(
      std::string value);
};

} // namespace openpuzzle::client

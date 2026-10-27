#pragma once

#include "openpuzzle/client/ClientHeartbeat.hpp"
#include "openpuzzle/client/ClientRegistration.hpp"
#include "openpuzzle/client/RangeAssignment.hpp"
#include "openpuzzle/client/RangeClaimResult.hpp"

#include <optional>
#include <functional>
#include <string>

namespace openpuzzle::client {

struct HttpPostResult { int status=0; std::string body,error; };
using HttpPostTransport=std::function<HttpPostResult(const std::string&,const std::string&)>;

class HttpRangeClient {
public:
  explicit HttpRangeClient(std::string serverUrl);
  HttpRangeClient(std::string serverUrl, HttpPostTransport transport);

  bool registerClient(const ClientRegistration &registration);

  bool heartbeat(const ClientHeartbeat &heartbeat);

  bool reportSolution(
      const std::string &assignmentId,
      const std::string &clientId);

  std::optional<RangeAssignment> claim(
      const std::string &clientId,
      int puzzle,
      int targetDurationMinutes,
      double speedMKeys = 0.0,
      const std::string &backend = {},
      const std::string &searchMode = "linear");

  RangeClaimResult claimResult(
      const std::string &clientId,
      int puzzle,
      int targetDurationMinutes,
      double speedMKeys = 0.0,
      const std::string &backend = {},
      const std::string &searchMode = "linear");

  bool complete(
      const std::string &assignmentId,
      const std::string &clientId,
      int exitCode,
      const std::string &status = "completed",
      const std::string &keysChecked = {});

  bool progress(const std::string &assignmentId, const std::string &clientId,
                double speedMKeys, const std::string &keysChecked);

  static std::optional<RangeAssignment>
  parseClaimResponse(const std::string &response, std::string &error);

  static RangeClaimResult
  parseClaimResult(const std::string &response);

  static std::string
  parseErrorCode(const std::string &response);

  static std::string
  parseErrorReason(const std::string &response);

  static std::string
  buildSolutionReportPayload(
      const std::string &assignmentId,
      const std::string &clientId);

  const std::string &lastError() const;
  const std::string &lastErrorCode() const;

private:
  std::string serverUrl_;
  HttpPostTransport transport_;
  std::string lastError_;
  std::string lastErrorCode_;

  RangeClaimStatus lastClaimStatus_ =
      RangeClaimStatus::Failed;

  static std::string shellQuote(const std::string &value);

  static std::string normalizeServerUrl(std::string value);
};

} // namespace openpuzzle::client

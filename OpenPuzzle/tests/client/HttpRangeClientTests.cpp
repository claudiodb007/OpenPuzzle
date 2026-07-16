#include "openpuzzle/client/HttpRangeClient.hpp"

#include <cassert>
#include <iostream>
#include <string>

using namespace openpuzzle::client;

int main() {
  const std::string json = R"JSON(
{
  "assignment_id": "assignment-71-000001",
  "puzzle": 71,
  "range_id": 84521,
  "target": "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU",
  "start": "400000070000000000",
  "end": "40000007FFFFFFFFFF"
}
)JSON";

  std::string error;

  const auto assignment =
      HttpRangeClient::parseClaimResponse(
          json,
          error);

  assert(assignment);
  assert(error.empty());
  assert(assignment->valid());

  assert(
      assignment->assignmentId ==
      "assignment-71-000001");

  assert(assignment->puzzle == 71);
  assert(assignment->rangeId == 84521);

  assert(
      assignment->start ==
      "400000070000000000");

  assert(
      assignment->end ==
      "40000007FFFFFFFFFF");

  const auto invalid =
      HttpRangeClient::parseClaimResponse(
          "{}",
          error);

  assert(!invalid);
  assert(!error.empty());

  const auto unavailable =
      HttpRangeClient::parseClaimResponse(
          R"JSON({
            "available": false,
            "message": "No work available"
          })JSON",
          error);

  assert(!unavailable);

  assert(
      error ==
      "No work available");

  const auto assignedResult =
      HttpRangeClient::parseClaimResult(
          json);

  assert(assignedResult.assigned());
  assert(!assignedResult.unavailable());
  assert(!assignedResult.failed());
  assert(assignedResult.assignment);
  assert(assignedResult.message.empty());

  const auto unavailableResult =
      HttpRangeClient::parseClaimResult(
          R"JSON({
            "available": false,
            "message": "No work available"
          })JSON");

  assert(!unavailableResult.assigned());
  assert(unavailableResult.unavailable());
  assert(!unavailableResult.failed());
  assert(!unavailableResult.assignment);
  assert(
      unavailableResult.message ==
      "No work available");

  const auto failedResult =
      HttpRangeClient::parseClaimResult(
          "{}");

  assert(!failedResult.assigned());
  assert(!failedResult.unavailable());
  assert(failedResult.failed());
  assert(!failedResult.assignment);
  assert(!failedResult.message.empty());

  std::cout
      << "HttpRangeClientTests passed\n";

  return 0;
}

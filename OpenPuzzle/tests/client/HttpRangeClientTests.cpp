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
  assert(assignment->searchMode == "linear");
  assert(assignment->publicKey.empty());
  assert(assignment->requiredBackend.empty());

  assert(
      assignment->start ==
      "400000070000000000");

  assert(
      assignment->end ==
      "40000007FFFFFFFFFF");


  {
    const std::string syntheticKangarooJson = R"JSON(
{
  "assignment_id": "assignment-synthetic-140",
  "puzzle": 140,
  "range_id": 90001,
  "target": "synthetic-target",
  "search_mode": "kangaroo",
  "public_key": "031f6a332d3c5c4f2de2378c012f429cd109ba07d69690c6c701b6bb87860d6640",
  "required_backend": "cuda",
  "start": "100000000",
  "end": "1FFFFFFFF"
}
)JSON";

    const auto kangaroo =
        HttpRangeClient::parseClaimResponse(
            syntheticKangarooJson,
            error);

    assert(kangaroo);
    assert(kangaroo->searchMode == "kangaroo");
    assert(
        kangaroo->publicKey ==
        "031f6a332d3c5c4f2de2378c012f429cd109ba07d69690c6c701b6bb87860d6640");
    assert(kangaroo->requiredBackend == "cuda");
  }



  {
    const auto invalidShape =
        HttpRangeClient::parseClaimResult(R"JSON({
          "assignment_id": "assignment-invalid-shape",
          "puzzle": 140,
          "range_id": 90002,
          "target": "synthetic-target",
          "search_mode": "kangaroo",
          "public_key": "031f6a332d3c5c4f2de2378c012f429cd109ba07d69690c6c701b6bb87860d6640",
          "required_backend": "cuda",
          "start": "100000000",
          "end": "1FFFFFFFE"
        })JSON");

    assert(invalidShape.failed());
    assert(!invalidShape.assignment);
    assert(
        invalidShape.message ==
        "Server returned invalid Kangaroo metadata or range shape");
  }

  {
    const auto invalidBackend =
        HttpRangeClient::parseClaimResult(R"JSON({
          "assignment_id": "assignment-invalid-backend",
          "puzzle": 140,
          "range_id": 90003,
          "target": "synthetic-target",
          "search_mode": "kangaroo",
          "public_key": "031f6a332d3c5c4f2de2378c012f429cd109ba07d69690c6c701b6bb87860d6640",
          "required_backend": "opencl",
          "start": "100000000",
          "end": "1FFFFFFFF"
        })JSON");

    assert(invalidBackend.failed());
    assert(!invalidBackend.assignment);
  }

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

  assert(
      HttpRangeClient::parseErrorCode(
          R"JSON({
            "error": "assignment_lease_expired",
            "message": "Assignment lease has expired"
          })JSON") ==
      "assignment_lease_expired");

  const std::string pausedResponse =
      R"JSON({
        "error": "invalid_assignment_state",
        "reason": "puzzle_verification_pending",
        "message": "Puzzle is paused while a potential solution is reviewed",
        "stop_requested": true
      })JSON";

  assert(
      HttpRangeClient::parseErrorCode(
          pausedResponse) ==
      "invalid_assignment_state");

  assert(
      HttpRangeClient::parseErrorReason(
          pausedResponse) ==
      "puzzle_verification_pending");

  assert(
      HttpRangeClient::parseErrorCode(
          "{}").empty());

  assert(
      HttpRangeClient::parseErrorReason(
          "{}").empty());

  /*
   * keys_checked inválido deve falhar antes
   * de qualquer pedido HTTP.
   */
  {
    HttpRangeClient client(
        "http://127.0.0.1:1");

    assert(
        !client.complete(
            "assignment-test",
            "client-test",
            -2,
            "cancelled",
            "12x34"));

    assert(
        client.lastError() ==
        "Keys checked must contain only digits");
  }


  /*
   * O relatório contém somente as identidades
   * públicas necessárias.
   */
  {
    const auto payload =
        HttpRangeClient::
            buildSolutionReportPayload(
                "assignment-public-id",
                "client-public-id");

    assert(
        payload ==
        "{\"assignment_id\":"
        "\"assignment-public-id\","
        "\"client_id\":"
        "\"client-public-id\"}"
    );

    assert(
        payload.find("private_key") ==
        std::string::npos
    );

    assert(
        payload.find("found.txt") ==
        std::string::npos
    );

    assert(
        payload.find("path") ==
        std::string::npos
    );
  }

  /*
   * Identidades vazias falham antes da rede.
   */
  {
    HttpRangeClient client(
        "http://127.0.0.1:1");

    assert(
        !client.reportSolution(
            "",
            "client-public-id")
    );

    assert(
        client.lastError() ==
        "Assignment identity is empty"
    );
  }

  std::cout
      << "HttpRangeClientTests passed\n";

  return 0;
}

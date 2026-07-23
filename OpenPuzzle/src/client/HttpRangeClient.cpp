#include "openpuzzle/client/HttpRangeClient.hpp"

#include "openpuzzle/client/ClientStateStore.hpp"

#include <cstdio>
#include <iomanip>
#include <regex>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <utility>

namespace openpuzzle::client {

namespace {

std::string jsonEscape(const std::string &value) {
  std::string result;

  result.reserve(value.size() + 8);

  for (const char character : value) {
    switch (character) {
    case '\\':
      result += "\\\\";
      break;

    case '"':
      result += "\\\"";
      break;

    case '\n':
      result += "\\n";
      break;

    case '\r':
      result += "\\r";
      break;

    case '\t':
      result += "\\t";
      break;

    default:
      result += character;
      break;
    }
  }

  return result;
}

bool extractString(const std::string &json, const std::string &key,
                   std::string &value) {
  const std::regex expression("\"" + key +
                              "\"\\s*:\\s*\"((?:\\\\.|[^\"\\\\])*)\"");

  std::smatch match;

  if (!std::regex_search(json, match, expression)) {
    return false;
  }

  value = match[1].str();

  return true;
}

bool extractInteger(const std::string &json, const std::string &key,
                    int &value) {
  const std::regex expression("\"" + key + "\"\\s*:\\s*(-?[0-9]+)");

  std::smatch match;

  if (!std::regex_search(json, match, expression)) {
    return false;
  }

  try {
    value = std::stoi(match[1].str());

    return true;
  } catch (...) {
    return false;
  }
}

bool extractBoolean(const std::string &json, const std::string &key,
                    bool &value) {
  const std::regex expression("\"" + key + "\"\\s*:\\s*(true|false)");

  std::smatch match;

  if (!std::regex_search(json, match, expression)) {
    return false;
  }

  value = match[1].str() == "true";

  return true;
}

} // namespace

HttpRangeClient::HttpRangeClient(std::string serverUrl)
    : serverUrl_(normalizeServerUrl(std::move(serverUrl))) {}

std::string HttpRangeClient::normalizeServerUrl(std::string value) {
  while (!value.empty() && value.back() == '/') {
    value.pop_back();
  }

  return value;
}

std::string HttpRangeClient::shellQuote(const std::string &value) {
  std::string quoted = "'";

  for (const char character : value) {
    if (character == '\'') {
      quoted += "'\\''";
    } else {
      quoted += character;
    }
  }

  quoted += '\'';

  return quoted;
}

RangeClaimResult
HttpRangeClient::parseClaimResult(
    const std::string &response) {
  RangeClaimResult result;

  bool available = true;

  if (extractBoolean(
          response,
          "available",
          available) &&
      !available) {
    result.status =
        RangeClaimStatus::Unavailable;

    if (!extractString(
            response,
            "message",
            result.message)) {
      result.message =
          "No assignment available";
    }

    return result;
  }

  RangeAssignment assignment;

  extractString(
      response,
      "assignment_id",
      assignment.assignmentId);

  extractInteger(
      response,
      "puzzle",
      assignment.puzzle);

  extractInteger(
      response,
      "range_id",
      assignment.rangeId);

  extractString(
      response,
      "target",
      assignment.target);

  extractString(
      response,
      "start",
      assignment.start);

  extractString(
      response,
      "end",
      assignment.end);

  if (!assignment.valid()) {
    result.status =
        RangeClaimStatus::Failed;

    result.message =
        "Server returned an invalid range assignment";

    return result;
  }

  result.status =
      RangeClaimStatus::Assigned;

  result.assignment =
      std::move(assignment);

  return result;
}

std::optional<RangeAssignment>
HttpRangeClient::parseClaimResponse(
    const std::string &response,
    std::string &error) {
  auto result =
      parseClaimResult(response);

  error = result.message;

  return result.assignment;
}

std::string HttpRangeClient::parseErrorCode(
    const std::string &response) {
  std::string errorCode;

  extractString(
      response,
      "error",
      errorCode);

  return errorCode;
}

std::string
HttpRangeClient::buildSolutionReportPayload(
    const std::string &assignmentId,
    const std::string &clientId) {
  std::ostringstream request;

  /*
   * Este payload é deliberadamente mínimo.
   *
   * Caminhos locais, conteúdo de found.txt,
   * chave privada e evidências não são aceites.
   */
  request
      << "{"
      << "\"assignment_id\":\""
      << jsonEscape(assignmentId)
      << "\","
      << "\"client_id\":\""
      << jsonEscape(clientId)
      << "\""
      << "}";

  return request.str();
}

bool HttpRangeClient::reportSolution(
    const std::string &assignmentId,
    const std::string &clientId) {
  lastError_.clear();
  lastErrorCode_.clear();

  if (serverUrl_.empty()) {
    lastError_ =
        "Server URL is empty";

    return false;
  }

  if (assignmentId.empty()) {
    lastError_ =
        "Assignment identity is empty";

    return false;
  }

  if (clientId.empty()) {
    lastError_ =
        "Client identity is empty";

    return false;
  }

  const std::string request =
      buildSolutionReportPayload(
          assignmentId,
          clientId);

  const std::string url =
      serverUrl_ +
      "/api/puzzle/report-solution";

  std::ostringstream command;

  command
      << "curl"
      << " --silent"
      << " --show-error"
      << " --location"
      << " --post301"
      << " --post302"
      << " --fail-with-body"
      << " --connect-timeout 10"
      << " --max-time 30"
      << " --request POST"
      << " --header "
      << shellQuote(
             "Content-Type: application/json")
      << " --data "
      << shellQuote(request)
      << ' '
      << shellQuote(url)
      << " 2>&1";

  FILE *pipe =
      popen(
          command.str().c_str(),
          "r");

  if (!pipe) {
    lastError_ =
        "Unable to start curl";

    return false;
  }

  char buffer[4096];

  std::string response;

  while (
      fgets(
          buffer,
          sizeof(buffer),
          pipe) != nullptr
  ) {
    response += buffer;
  }

  const int result =
      pclose(pipe);

  if (result == -1) {
    lastError_ =
        "Unable to obtain curl result";

    return false;
  }

  if (
      !WIFEXITED(result) ||
      WEXITSTATUS(result) != 0
  ) {
    lastErrorCode_ =
        parseErrorCode(response);

    lastError_ =
        response.empty()
            ? "HTTP request failed"
            : response;

    while (
        !lastError_.empty() &&
        (
            lastError_.back() == '\n' ||
            lastError_.back() == '\r'
        )
    ) {
      lastError_.pop_back();
    }

    return false;
  }

  return true;
}

bool HttpRangeClient::registerClient(const ClientRegistration &registration) {
  lastError_.clear();

  if (serverUrl_.empty()) {
    lastError_ = "Server URL is empty";

    return false;
  }

  if (!registration.valid()) {
    lastError_ = "Client registration is invalid";

    return false;
  }

  std::ostringstream request;

  request << "{"
          << "\"client_id\":\"" << jsonEscape(registration.clientId) << "\""
          << "}";

  const std::string url = serverUrl_ + "/api/client/register";

  std::ostringstream command;

  command << "curl"
          << " --silent"
          << " --show-error"
          << " --location"
          << " --post301"
          << " --post302"
          << " --fail-with-body"
          << " --connect-timeout 10"
          << " --max-time 30"
          << " --request POST"
          << " --header " << shellQuote("Content-Type: application/json")
          << " --data " << shellQuote(request.str()) << ' ' << shellQuote(url)
          << " 2>&1";

  FILE *pipe = popen(command.str().c_str(), "r");

  if (!pipe) {
    lastError_ = "Unable to start curl";

    return false;
  }

  char buffer[4096];
  std::string response;

  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    response += buffer;
  }

  const int result = pclose(pipe);

  if (result == -1) {
    lastError_ = "Unable to obtain curl result";

    return false;
  }

  if (!WIFEXITED(result) || WEXITSTATUS(result) != 0) {
    lastError_ = response.empty() ? "HTTP request failed" : response;

    while (!lastError_.empty() &&
           (lastError_.back() == '\n' || lastError_.back() == '\r')) {
      lastError_.pop_back();
    }

    return false;
  }

  return true;
}

bool HttpRangeClient::heartbeat(const ClientHeartbeat &heartbeat) {
  lastError_.clear();

  if (serverUrl_.empty()) {
    lastError_ = "Server URL is empty";

    return false;
  }

  if (!heartbeat.valid()) {
    lastError_ = "Client heartbeat is invalid";

    return false;
  }

  std::ostringstream request;

  request << "{"
          << "\"client_id\":\"" << jsonEscape(heartbeat.clientId) << "\","
          << "\"version\":\"" << jsonEscape(heartbeat.version) << "\","
          << "\"platform\":\"" << jsonEscape(heartbeat.platform) << "\","
          << "\"status\":\"" << jsonEscape(heartbeat.status) << "\","
          << "\"active_engine\":\""
          << jsonEscape(heartbeat.activeEngine) << "\","
          << "\"active_backend\":\""
          << jsonEscape(heartbeat.activeBackend) << "\","
          << "\"active_backends\":[";

  for (std::size_t index = 0;
       index < heartbeat.activeBackends.size();
       ++index) {
    if (index > 0) {
      request << ',';
    }

    request
        << "\""
        << jsonEscape(
               heartbeat.activeBackends[index])
        << "\"";
  }

  request << "],"
          << "\"cpu\":{"
          << "\"name\":\"" << jsonEscape(heartbeat.cpu.name) << "\","
          << "\"cores\":" << heartbeat.cpu.cores << ","
          << "\"threads\":" << heartbeat.cpu.threads << "},"
          << "\"gpus\":[";

  for (std::size_t index = 0; index < heartbeat.gpus.size(); ++index) {
    const auto &gpu = heartbeat.gpus[index];

    if (index > 0) {
      request << ',';
    }

    request << "{"
            << "\"backend\":\"" << jsonEscape(gpu.backend) << "\","
            << "\"name\":\"" << jsonEscape(gpu.name) << "\","
            << "\"memory_mb\":" << gpu.memoryMB << "}";
  }

  request << "],"
          << "\"engines\":[";

  for (std::size_t index = 0; index < heartbeat.engines.size(); ++index) {
    const auto &engine = heartbeat.engines[index];

    if (index > 0) {
      request << ',';
    }

    request << "{"
            << "\"name\":\"" << jsonEscape(engine.name) << "\","
            << "\"backend\":\"" << jsonEscape(engine.backend) << "\","
            << "\"installed\":" << (engine.installed ? "true" : "false") << ","
            << "\"available\":" << (engine.available ? "true" : "false") << "}";
  }

  request << "]"
          << "}";

  const std::string url = serverUrl_ + "/api/client/heartbeat";

  std::ostringstream command;

  command << "curl"
          << " --silent"
          << " --show-error"
          << " --location"
          << " --post301"
          << " --post302"
          << " --fail-with-body"
          << " --connect-timeout 10"
          << " --max-time 30"
          << " --request POST"
          << " --header " << shellQuote("Content-Type: application/json")
          << " --data " << shellQuote(request.str()) << ' ' << shellQuote(url)
          << " 2>&1";

  FILE *pipe = popen(command.str().c_str(), "r");

  if (!pipe) {
    lastError_ = "Unable to start curl";

    return false;
  }

  char buffer[4096];
  std::string response;

  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    response += buffer;
  }

  const int result = pclose(pipe);

  if (result == -1) {
    lastError_ = "Unable to obtain curl result";

    return false;
  }

  if (!WIFEXITED(result) || WEXITSTATUS(result) != 0) {
    lastError_ = response.empty() ? "HTTP request failed" : response;

    while (!lastError_.empty() &&
           (lastError_.back() == '\n' || lastError_.back() == '\r')) {
      lastError_.pop_back();
    }

    return false;
  }

  return true;
}

std::optional<RangeAssignment>
HttpRangeClient::claim(const std::string &clientId, int puzzle,
                       int targetDurationMinutes, double speedMKeys) {
  lastError_.clear();

  lastClaimStatus_ =
      RangeClaimStatus::Failed;

  if (serverUrl_.empty()) {
    lastError_ = "Server URL is empty";

    return std::nullopt;
  }

  if (clientId.empty()) {
    lastError_ = "Client identity is empty";

    return std::nullopt;
  }

  if (puzzle < 0) {
    lastError_ = "Puzzle number is invalid";

    return std::nullopt;
  }

  if (targetDurationMinutes < 1 || targetDurationMinutes > 360) {
    lastError_ = "Target duration must be between 1 and 360 minutes";

    return std::nullopt;
  }

  if (speedMKeys < 0.0) {
    lastError_ = "Speed cannot be negative";

    return std::nullopt;
  }

  std::ostringstream request;

  request << "{"
          << "\"client_id\":\"" << jsonEscape(clientId) << "\","
          << "\"puzzle\":" << puzzle << ","
          << "\"execution_slot\":\""
          << jsonEscape(
                 ClientStateStore::executionSlot())
          << "\","
          << "\"target_duration_minutes\":" << targetDurationMinutes << ","
          << "\"speed_mkeys\":" << std::fixed << std::setprecision(6)
          << speedMKeys << "}";

  const std::string url = serverUrl_ + "/api/range/claim";

  std::ostringstream command;

  command << "curl"
          << " --silent"
          << " --show-error"
          << " --location"
          << " --post301"
          << " --post302"
          << " --fail-with-body"
          << " --connect-timeout 10"
          << " --max-time 30"
          << " --request POST"
          << " --header " << shellQuote("Content-Type: application/json")
          << " --data " << shellQuote(request.str()) << ' ' << shellQuote(url)
          << " 2>&1";

  FILE *pipe = popen(command.str().c_str(), "r");

  if (!pipe) {
    lastError_ = "Unable to start curl";

    return std::nullopt;
  }

  char buffer[4096];

  std::string response;

  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    response += buffer;
  }

  const int result = pclose(pipe);

  if (result == -1) {
    lastError_ = "Unable to obtain curl result";

    return std::nullopt;
  }

  if (!WIFEXITED(result) || WEXITSTATUS(result) != 0) {
    lastError_ = response.empty() ? "HTTP request failed" : response;

    while (!lastError_.empty() &&
           (lastError_.back() == '\n' || lastError_.back() == '\r')) {
      lastError_.pop_back();
    }

    return std::nullopt;
  }

  auto claimResult =
      parseClaimResult(response);

  lastClaimStatus_ =
      claimResult.status;

  lastError_ =
      claimResult.message;

  return claimResult.assignment;
}

RangeClaimResult HttpRangeClient::claimResult(
    const std::string &clientId,
    int puzzle,
    int targetDurationMinutes,
    double speedMKeys) {
  auto assignment =
      claim(
          clientId,
          puzzle,
          targetDurationMinutes,
          speedMKeys);

  RangeClaimResult result;

  result.status =
      lastClaimStatus_;

  result.assignment =
      std::move(assignment);

  result.message =
      lastError_;

  return result;
}

bool HttpRangeClient::progress(const std::string &assignmentId,
                               const std::string &clientId, double speedMKeys,
                               const std::string &keysChecked) {
  lastError_.clear();
  lastErrorCode_.clear();

  if (serverUrl_.empty()) {
    lastError_ = "Server URL is empty";

    return false;
  }

  if (assignmentId.empty()) {
    lastError_ = "Assignment identity is empty";

    return false;
  }

  if (clientId.empty()) {
    lastError_ = "Client identity is empty";

    return false;
  }

  if (speedMKeys < 0.0) {
    lastError_ = "Speed cannot be negative";

    return false;
  }

  if (keysChecked.empty()) {
    lastError_ = "Keys checked is empty";

    return false;
  }

  for (const char character : keysChecked) {
    if (character < '0' || character > '9') {
      lastError_ = "Keys checked must contain only digits";

      return false;
    }
  }

  std::ostringstream request;

  request << "{"
          << "\"assignment_id\":\"" << jsonEscape(assignmentId) << "\","
          << "\"client_id\":\"" << jsonEscape(clientId) << "\","
          << "\"status\":\"running\","
          << "\"speed_mkeys\":" << speedMKeys << ","
          << "\"keys_checked\":\"" << jsonEscape(keysChecked) << "\""
          << "}";

  const std::string url = serverUrl_ + "/api/range/progress";

  std::ostringstream command;

  command << "curl"
          << " --silent"
          << " --show-error"
          << " --location"
          << " --post301"
          << " --post302"
          << " --fail-with-body"
          << " --connect-timeout 10"
          << " --max-time 30"
          << " --request POST"
          << " --header " << shellQuote("Content-Type: application/json")
          << " --data " << shellQuote(request.str()) << ' ' << shellQuote(url)
          << " 2>&1";

  FILE *pipe = popen(command.str().c_str(), "r");

  if (!pipe) {
    lastError_ = "Unable to start curl";

    return false;
  }

  char buffer[4096];

  std::string response;

  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    response += buffer;
  }

  const int result = pclose(pipe);

  if (result == -1) {
    lastError_ = "Unable to obtain curl result";

    return false;
  }

  if (!WIFEXITED(result) || WEXITSTATUS(result) != 0) {
    lastErrorCode_ =
        parseErrorCode(response);

    lastError_ = response.empty() ? "HTTP request failed" : response;

    while (!lastError_.empty() &&
           (lastError_.back() == '\n' || lastError_.back() == '\r')) {
      lastError_.pop_back();
    }

    return false;
  }

  return true;
}

bool HttpRangeClient::complete(
    const std::string &assignmentId,
    const std::string &clientId,
    int exitCode,
    const std::string &status,
    const std::string &keysChecked) {
  lastError_.clear();
  lastErrorCode_.clear();

  if (serverUrl_.empty()) {
    lastError_ = "Server URL is empty";

    return false;
  }

  if (assignmentId.empty()) {
    lastError_ = "Assignment identity is empty";

    return false;
  }

  if (clientId.empty()) {
    lastError_ = "Client identity is empty";

    return false;
  }

  if (status != "completed" && status != "failed" && status != "cancelled") {
    lastError_ = "Status must be completed, failed or cancelled";

    return false;
  }

  if (status == "completed" && exitCode != 0) {
    lastError_ = "Completed executions must use exit code 0";

    return false;
  }

  if (status != "completed" && exitCode == 0) {
    lastError_ =
        "Failed and cancelled executions must use a non-zero exit code";

    return false;
  }

  if (!keysChecked.empty()) {
    for (const char character : keysChecked) {
      if (character < '0' ||
          character > '9') {
        lastError_ =
            "Keys checked must contain only digits";

        return false;
      }
    }
  }

  std::ostringstream request;

  request << "{"
          << "\"assignment_id\":\"" << jsonEscape(assignmentId) << "\","
          << "\"client_id\":\"" << jsonEscape(clientId) << "\","
          << "\"exit_code\":" << exitCode << ","
          << "\"status\":\"" << jsonEscape(status) << "\"";

  if (!keysChecked.empty()) {
    request
        << ",\"keys_checked\":\""
        << jsonEscape(keysChecked)
        << "\"";
  }

  request << "}";

  const std::string url = serverUrl_ + "/api/range/complete";

  std::ostringstream command;

  command << "curl"
          << " --silent"
          << " --show-error"
          << " --location"
          << " --post301"
          << " --post302"
          << " --fail-with-body"
          << " --connect-timeout 10"
          << " --max-time 30"
          << " --request POST"
          << " --header " << shellQuote("Content-Type: application/json")
          << " --data " << shellQuote(request.str()) << ' ' << shellQuote(url)
          << " 2>&1";

  FILE *pipe = popen(command.str().c_str(), "r");

  if (!pipe) {
    lastError_ = "Unable to start curl";

    return false;
  }

  char buffer[4096];

  std::string response;

  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    response += buffer;
  }

  const int result = pclose(pipe);

  if (result == -1) {
    lastError_ = "Unable to obtain curl result";

    return false;
  }

  if (!WIFEXITED(result) || WEXITSTATUS(result) != 0) {
    lastErrorCode_ =
        parseErrorCode(response);

    lastError_ = response.empty() ? "HTTP request failed" : response;

    while (!lastError_.empty() &&
           (lastError_.back() == '\n' || lastError_.back() == '\r')) {
      lastError_.pop_back();
    }

    return false;
  }

  return true;
}

const std::string &HttpRangeClient::lastError() const { return lastError_; }

const std::string &
HttpRangeClient::lastErrorCode() const {
  return lastErrorCode_;
}

} // namespace openpuzzle::client

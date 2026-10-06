#include "openpuzzle/core/RecoveryManager.hpp"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <charconv>
#include <cmath>
#include <filesystem>
#include <locale>
#include <optional>
#include <sstream>
#include <string>

namespace openpuzzle {

namespace {
std::optional<boost::property_tree::ptree> readDocument(
    const std::filesystem::path& path) {
  try {
    boost::property_tree::ptree document;
    boost::property_tree::read_json(path.string(), document);
    return document;
  } catch (...) {
    return std::nullopt;
  }
}

std::string extractString(const boost::property_tree::ptree& document,
                          const std::string& key,
                          const std::string& fallback) {
  const auto value = document.get_optional<std::string>(key);
  return value ? *value : fallback;
}

template <typename Number>
Number extractInteger(const boost::property_tree::ptree& document,
                      const std::string& key, Number fallback) {
  const auto text = extractString(document, key, "");
  Number value = 0;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size()
      ? value : fallback;
}

bool extractBool(const boost::property_tree::ptree& document,
                 const std::string& key, bool fallback) {
  const auto text = extractString(document, key, "");
  return text == "true" ? true : text == "false" ? false : fallback;
}

double extractDouble(const boost::property_tree::ptree& document,
                     const std::string& key, double fallback) {
  std::istringstream input(extractString(document, key, ""));
  input.imbue(std::locale::classic());
  double value = 0.0;
  if (!(input >> value) || !std::isfinite(value)) {
    return fallback;
  }
  input >> std::ws;
  return input.eof() ? value : fallback;
}
} // namespace

RecoveryManager::RecoveryManager(WorkspaceManager workspaceManager)
    : workspaceManager_(std::move(workspaceManager)) {}

bool RecoveryManager::hasStateFile(int jobId) const {
  return std::filesystem::exists(workspaceManager_.stateFile(jobId));
}

RecoveryState RecoveryManager::load(int jobId) const {
  RecoveryState state;
  const auto document = readDocument(workspaceManager_.stateFile(jobId));
  if (!document) {
    return state;
  }

  state.jobId = jobId;
  const auto status = extractString(*document, "status", "");
  if (status == "FINISHED") {
    state.status = RecoveryStatus::Finished;
  } else if (status == "FAILED") {
    state.status = RecoveryStatus::Failed;
  } else if (status == "RUNNING") {
    state.status = RecoveryStatus::Running;
  }
  state.exitCode = extractInteger(*document, "exit_code", -1);
  state.linesRead = extractInteger(*document, "lines_read", std::uint64_t{0});
  state.averageSpeed = extractDouble(*document, "average_speed", 0.0);
  return state;
}

ExecutionContext RecoveryManager::buildExecutionContext(int jobId) const {
  ExecutionContext ctx;
  ctx.jobId = jobId;
  ctx.workspace = workspaceManager_.jobWorkspace(jobId).string();
  const auto document = readDocument(workspaceManager_.executionFile(jobId));
  if (!document) {
    return ctx;
  }

  ctx.executionId = extractInteger(*document, "execution_id", 0);
  ctx.puzzleId = extractInteger(*document, "puzzle_id", 0);
  ctx.jobId = extractInteger(*document, "job_id", jobId);
  ctx.rangeId = extractInteger(*document, "range_id", 0);
  ctx.engine = extractString(*document, "engine", "");
  ctx.command = extractString(*document, "command", "");
  ctx.workspace = extractString(
      *document, "workspace", workspaceManager_.jobWorkspace(ctx.jobId).string());
  ctx.echoOutput = extractBool(*document, "echo_output", true);
  return ctx;
}

} // namespace openpuzzle

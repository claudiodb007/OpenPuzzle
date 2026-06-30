#include "openpuzzle/core/RecoveryManager.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace openpuzzle {

RecoveryManager::RecoveryManager(WorkspaceManager workspaceManager)
    : workspaceManager_(std::move(workspaceManager)) {}

bool RecoveryManager::hasStateFile(int jobId) const {
  return std::filesystem::exists(workspaceManager_.stateFile(jobId));
}

static std::string readFile(const std::filesystem::path &path) {
  std::ifstream in(path);

  if (!in.is_open()) {
    return {};
  }

  std::stringstream buffer;
  buffer << in.rdbuf();

  return buffer.str();
}

static bool contains(const std::string &text, const std::string &token) {
  return text.find(token) != std::string::npos;
}

static int extractInt(const std::string &text, const std::string &key,
                      int defaultValue) {
  auto pos = text.find(key);

  if (pos == std::string::npos) {
    return defaultValue;
  }

  pos = text.find(':', pos);

  if (pos == std::string::npos) {
    return defaultValue;
  }

  try {
    return std::stoi(text.substr(pos + 1));
  } catch (...) {
    return defaultValue;
  }
}

static std::uint64_t extractUInt64(const std::string &text,
                                   const std::string &key,
                                   std::uint64_t defaultValue) {
  auto pos = text.find(key);

  if (pos == std::string::npos) {
    return defaultValue;
  }

  pos = text.find(':', pos);

  if (pos == std::string::npos) {
    return defaultValue;
  }

  try {
    return static_cast<std::uint64_t>(std::stoull(text.substr(pos + 1)));
  } catch (...) {
    return defaultValue;
  }
}

static double extractDouble(const std::string &text, const std::string &key,
                            double defaultValue) {
  auto pos = text.find(key);

  if (pos == std::string::npos) {
    return defaultValue;
  }

  pos = text.find(':', pos);

  if (pos == std::string::npos) {
    return defaultValue;
  }

  try {
    return std::stod(text.substr(pos + 1));
  } catch (...) {
    return defaultValue;
  }
}

RecoveryState RecoveryManager::load(int jobId) const {
  RecoveryState state;

  auto content = readFile(workspaceManager_.stateFile(jobId));

  if (content.empty()) {
    return state;
  }

  state.jobId = jobId;

  if (contains(content, "\"status\": \"FINISHED\"")) {
    state.status = RecoveryStatus::Finished;
  } else if (contains(content, "\"status\": \"FAILED\"")) {
    state.status = RecoveryStatus::Failed;
  } else if (contains(content, "\"status\": \"RUNNING\"")) {
    state.status = RecoveryStatus::Running;
  } else {
    state.status = RecoveryStatus::Unknown;
  }

  state.exitCode = extractInt(content, "\"exit_code\"", -1);
  state.linesRead = extractUInt64(content, "\"lines_read\"", 0);
  state.averageSpeed = extractDouble(content, "\"average_speed\"", 0.0);

  return state;
}

} // namespace openpuzzle

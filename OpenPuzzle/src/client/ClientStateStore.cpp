#include "openpuzzle/client/ClientStateStore.hpp"

#include "openpuzzle/runtime/WorkspaceSecurity.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>

namespace openpuzzle::client {

namespace {

std::string escapeValue(
    const std::string& value) {
  std::string escaped;

  for (const char character : value) {
    switch (character) {
    case '\\':
      escaped += "\\\\";
      break;

    case '\n':
      escaped += "\\n";
      break;

    case '\r':
      break;

    default:
      escaped += character;
      break;
    }
  }

  return escaped;
}

std::string unescapeValue(
    const std::string& value) {
  std::string result;

  bool escaped = false;

  for (const char character : value) {
    if (escaped) {
      if (character == 'n') {
        result += '\n';
      } else {
        result += character;
      }

      escaped = false;
      continue;
    }

    if (character == '\\') {
      escaped = true;
      continue;
    }

    result += character;
  }

  if (escaped) {
    result += '\\';
  }

  return result;
}

void writeField(
    std::ofstream& output,
    const std::string& name,
    const std::string& value) {
  output
      << name
      << '='
      << escapeValue(value)
      << '\n';
}

int parseInteger(
    const std::map<std::string, std::string>& values,
    const std::string& name) {
  const auto iterator =
      values.find(name);

  if (iterator == values.end() ||
      iterator->second.empty()) {
    return 0;
  }

  try {
    return std::stoi(
        iterator->second);
  } catch (...) {
    return 0;
  }
}

std::string valueOf(
    const std::map<std::string, std::string>& values,
    const std::string& name) {
  const auto iterator =
      values.find(name);

  if (iterator == values.end()) {
    return {};
  }

  return iterator->second;
}

} // namespace

std::string
ClientStateStore::executionSlot() {
  const char* value =
      std::getenv(
          "OPENPUZZLE_EXECUTION_SLOT");

  if (value != nullptr) {
    const std::string slot(value);

    if (slot == "gpu" ||
        slot == "cpu") {
      return slot;
    }
  }

  return "primary";
}

std::filesystem::path
ClientStateStore::path() {
  return path(
      executionSlot());
}

std::filesystem::path
ClientStateStore::path(
    const std::string& executionSlot) {
  const char* home =
      std::getenv("HOME");

  const std::filesystem::path root =
      home
          ? std::filesystem::path(home)
          : std::filesystem::current_path();

  std::string filename =
      "client.state";

  if (executionSlot == "gpu") {
    filename = "client-gpu.state";
  } else if (executionSlot == "cpu") {
    filename = "client-cpu.state";
  }

  return root /
         ".local" /
         "share" /
         "OpenPuzzle" /
         filename;
}

bool ClientStateStore::save(
    const ClientExecutionState& state) {
  return save(
      state,
      executionSlot());
}

bool ClientStateStore::save(
    const ClientExecutionState& state,
    const std::string& executionSlot) {
  if (!state.valid()) {
    return false;
  }

  const auto statePath =
      path(executionSlot);

  try {
    WorkspaceSecurity::prepare(
        statePath.parent_path());
  } catch (...) {
    return false;
  }

  const auto temporaryPath =
      statePath.string() +
      ".tmp";

  std::ofstream output(
      temporaryPath,
      std::ios::trunc);

  if (!output) {
    return false;
  }

  try {
    WorkspaceSecurity::protectFile(
        temporaryPath);
  } catch (...) {
    output.close();

    std::filesystem::remove(
        temporaryPath);

    return false;
  }

  output
      << "active=1\n"
      << "puzzle="
      << state.puzzle
      << '\n'
      << "range_id="
      << state.rangeId
      << '\n'
      << "pid="
      << state.pid
      << '\n'
      << "threads="
      << state.threads
      << '\n';

  writeField(
      output,
      "assignment_id",
      state.assignmentId);

  writeField(
      output,
      "client_id",
      state.clientId);

  writeField(
      output,
      "target",
      state.target);

  writeField(
      output,
      "start",
      state.start);

  writeField(
      output,
      "end",
      state.end);

  writeField(
      output,
      "engine",
      state.engine);

  writeField(
      output,
      "backend",
      state.backend);

  writeField(
      output,
      "workspace",
      state.workspace);

  writeField(
      output,
      "command",
      state.command);

  output.close();

  if (!output) {
    std::filesystem::remove(
        temporaryPath);

    return false;
  }

  std::error_code error;

  std::filesystem::rename(
      temporaryPath,
      statePath,
      error);

  if (!error) {
    return true;
  }

  std::filesystem::remove(
      statePath,
      error);

  error.clear();

  std::filesystem::rename(
      temporaryPath,
      statePath,
      error);

  return !error;
}

std::optional<ClientExecutionState>
ClientStateStore::load() {
  return load(
      executionSlot());
}

std::optional<ClientExecutionState>
ClientStateStore::load(
    const std::string& executionSlot) {
  const auto statePath =
      path(executionSlot);

  try {
    WorkspaceSecurity::prepare(
        statePath.parent_path());

    if (std::filesystem::is_regular_file(
            statePath)) {
      WorkspaceSecurity::protectFile(
          statePath);
    }
  } catch (...) {
    return std::nullopt;
  }

  std::ifstream input(
      statePath);

  if (!input) {
    return std::nullopt;
  }

  std::map<std::string, std::string>
      values;

  std::string line;

  while (std::getline(
      input,
      line)) {
    const auto separator =
        line.find('=');

    if (separator ==
        std::string::npos) {
      continue;
    }

    const auto name =
        line.substr(
            0,
            separator);

    const auto rawValue =
        line.substr(
            separator + 1);

    values[name] =
        unescapeValue(
            rawValue);
  }

  ClientExecutionState state;

  state.active =
      valueOf(
          values,
          "active") == "1";

  state.assignmentId =
      valueOf(
          values,
          "assignment_id");

  state.clientId =
      valueOf(
          values,
          "client_id");

  state.puzzle =
      parseInteger(
          values,
          "puzzle");

  state.rangeId =
      parseInteger(
          values,
          "range_id");

  state.pid =
      parseInteger(
          values,
          "pid");

  state.threads =
      parseInteger(
          values,
          "threads");

  state.target =
      valueOf(
          values,
          "target");

  state.start =
      valueOf(
          values,
          "start");

  state.end =
      valueOf(
          values,
          "end");

  state.engine =
      valueOf(
          values,
          "engine");

  state.backend =
      valueOf(
          values,
          "backend");

  state.workspace =
      valueOf(
          values,
          "workspace");

  state.command =
      valueOf(
          values,
          "command");

  if (!state.valid()) {
    return std::nullopt;
  }

  return state;
}

bool ClientStateStore::remove() {
  return remove(
      executionSlot());
}

bool ClientStateStore::remove(
    const std::string& executionSlot) {
  std::error_code error;

  const bool removed =
      std::filesystem::remove(
          path(executionSlot),
          error);

  return removed || !error;
}

} // namespace openpuzzle::client

#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/client/ClientStateStore.hpp"

#include "openpuzzle/runtime/ExecutionSlot.hpp"
#include "openpuzzle/runtime/WorkspaceSecurity.hpp"
#include "openpuzzle/core/AtomicFile.hpp"

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <locale>
#include <sstream>
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
      escaped += "\\r";
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
      } else if (character == 'r') {
        result += '\r';
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
    std::ostream& output,
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

  int value = 0;
  const auto& text = iterator->second;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size()
      ? value : 0;
}

std::uint64_t parseUnsigned64(
    const std::map<std::string, std::string>& values,
    const std::string& name) {
  const auto iterator =
      values.find(name);

  if (iterator == values.end() ||
      iterator->second.empty()) {
    return 0;
  }

  std::uint64_t value = 0;
  const auto& text = iterator->second;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size()
      ? value : 0;
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
#ifdef _WIN32
  // Use the current OS environment, consistent with the Windows launch and
  // control APIs. The CRT may retain its own earlier environment snapshot.
  char slot[64]{};
  const DWORD count = GetEnvironmentVariableA(
      "OPENPUZZLE_EXECUTION_SLOT", slot, sizeof(slot));
  if (count > 0 && count < sizeof(slot) &&
      ExecutionSlot::valid(std::string(slot, count))) {
    return std::string(slot, count);
  }
#else
  const char* value =
      std::getenv(
          "OPENPUZZLE_EXECUTION_SLOT");

  if (value != nullptr) {
    const std::string slot(value);

    if (ExecutionSlot::valid(slot)) {
      return slot;
    }
  }
#endif

  return "primary";
}

std::string ClientStateStore::currentBootId() {
#ifdef _WIN32
  // Windows process identity will use creation time, never Linux boot IDs.
  return {};
#else
  std::ifstream input(
      "/proc/sys/kernel/random/boot_id");

  if (!input) {
    return {};
  }

  std::string bootId;

  if (!std::getline(input, bootId)) {
    return {};
  }

  while (!bootId.empty() &&
         (bootId.back() == '\r' ||
          bootId.back() == '\n' ||
          bootId.back() == ' ' ||
          bootId.back() == '\t')) {
    bootId.pop_back();
  }

  return bootId;
#endif
}

std::filesystem::path
ClientStateStore::path() {
  return path(
      executionSlot());
}

std::filesystem::path
ClientStateStore::path(
    const std::string& executionSlot) {
  const std::string filename =
      "client" +
      ExecutionSlot::fileSuffix(executionSlot) +
      ".state";

  return localPaths::dataDirectory() / filename;
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

  std::filesystem::path statePath;
  try {
    statePath = path(executionSlot);
    WorkspaceSecurity::prepare(
        statePath.parent_path());
  } catch (...) {
    return false;
  }

  std::ostringstream output;
  output.imbue(std::locale::classic());

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
      << '\n';

  writeField(
      output,
      "boot_id",
      state.bootId);

  writeField(output, "process_platform", state.processPlatform);
  writeField(output, "process_generation", state.processGeneration);
  output << "owner_pid=" << state.ownerPid << '\n'
         << "owner_start_time=" << state.ownerStartTime << '\n';

  output
      << "process_start_time="
      << state.processStartTime
      << '\n';

  output
      << "device="
      << state.device
      << '\n'
      << "blocks="
      << state.blocks
      << '\n'
      << "threads="
      << state.threads
      << '\n'
      << "points="
      << state.points
      << '\n'
      << "profile_managed="
      << (state.profileManaged ? 1 : 0)
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
      "public_key",
      state.publicKey);

  writeField(
      output,
      "kangaroo_walk_seed",
      state.kangarooWalkSeed);

  output
      << "kangaroo_generation="
      << state.kangarooGeneration
      << '\n';

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
      "gpu_name",
      state.gpuName);

  writeField(
      output,
      "workspace",
      state.workspace);

  writeField(
      output,
      "command",
      state.command);

  return writePrivateFileAtomically(statePath, output.str());
}

std::optional<ClientExecutionState>
ClientStateStore::load() {
  return load(
      executionSlot());
}

std::optional<ClientExecutionState>
ClientStateStore::load(
    const std::string& executionSlot) {
  std::filesystem::path statePath;
  try {
    statePath = path(executionSlot);
    // Reading an absent state must not initialize a profile. Directory
    // creation belongs to save(); protection only opens existing files.
    if (std::filesystem::is_regular_file(
            statePath)) {
      WorkspaceSecurity::protectFile(
          statePath);
    }
  } catch (...) {
    return std::nullopt;
  }

#ifdef _WIN32
  bool missing = false;
  const auto contents = windowsFile::readPrivate(statePath, missing);
  if (!contents) return std::nullopt;
  std::istringstream input(*contents);
#else
  std::ifstream input(
      statePath);
#endif

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
  state.processPlatform = valueOf(values, "process_platform");
  state.processGeneration = valueOf(values, "process_generation");
  state.ownerPid = parseInteger(values, "owner_pid");
  state.ownerStartTime = parseUnsigned64(values, "owner_start_time");

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

  state.bootId =
      valueOf(
          values,
          "boot_id");

  state.processStartTime =
      parseUnsigned64(
          values,
          "process_start_time");

  state.device =
      parseInteger(
          values,
          "device");

  state.blocks =
      parseInteger(
          values,
          "blocks");

  state.threads =
      parseInteger(
          values,
          "threads");

  state.points =
      parseInteger(
          values,
          "points");

  state.profileManaged =
      valueOf(
          values,
          "profile_managed") == "1";

  state.target =
      valueOf(
          values,
          "target");

  state.publicKey =
      valueOf(
          values,
          "public_key");

  state.kangarooWalkSeed =
      valueOf(
          values,
          "kangaroo_walk_seed");

  state.kangarooGeneration =
      parseUnsigned64(
          values,
          "kangaroo_generation");

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

  state.gpuName =
      valueOf(
          values,
          "gpu_name");

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
  try {
    const auto statePath = path(executionSlot);
#ifdef _WIN32
    if (!windowsFile::safePath(statePath)) return false;
#endif
    std::error_code error;
    const bool removed = std::filesystem::remove(statePath, error);
    return removed || !error;
  } catch (...) { return false; }
}

} // namespace openpuzzle::client

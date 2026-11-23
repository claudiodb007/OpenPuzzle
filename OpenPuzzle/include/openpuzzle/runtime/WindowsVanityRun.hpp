#pragma once

#include "openpuzzle/client/ClientExecutionState.hpp"
#include "openpuzzle/runtime/VanityRangeProtocol.hpp"
#ifdef _WIN32
#include "openpuzzle/core/LocalPaths.hpp"
#include "openpuzzle/core/WindowsPrivateFile.hpp"
#include <optional>
#endif

namespace openpuzzle {

inline vanityPreview::Plan windowsVanityPlan(const client::ClientExecutionState& state) {
  using namespace vanityPreview;
  const auto start = canonical(state.start);
  const auto end = canonical(state.end);
  if (end < start) throw std::invalid_argument("Invalid Vanity range");
  // Subtract the inclusive boundaries without truncating their high bits.
  std::uint64_t distance = 0;
  unsigned borrow = 0;
  const auto digit = [](char c) { return static_cast<unsigned>(c <= '9' ? c - '0' : c - 'a' + 10); };
  for (int i = 63; i >= 0; --i) {
    const int diff = static_cast<int>(digit(end[i])) - static_cast<int>(digit(start[i])) -
        static_cast<int>(borrow);
    borrow = diff < 0 ? 1 : 0;
    const auto value = static_cast<unsigned>(diff < 0 ? diff + 16 : diff);
    const auto position = 63 - i;
    if (position >= 16) {
      if (value != 0) throw std::invalid_argument("Vanity range exceeds native coverage limit");
    } else {
      distance |= static_cast<std::uint64_t>(value) << (position * 4);
    }
  }
  if (distance >= NativeRangeLimit || state.vanityResumeOffset > distance)
    throw std::invalid_argument("Vanity assignment or resume offset exceeds native coverage");
  return Plan(add(start, state.vanityResumeOffset), distance + 1 - state.vanityResumeOffset, state.blocks);
}

inline vanityPreview::Protocol windowsVanityEvidence(
    const client::ClientExecutionState& state, const std::string& log) {
  vanityPreview::Protocol protocol(windowsVanityPlan(state));
  std::size_t cursor = 0;
  while (cursor < log.size()) {
    const auto next = log.find('\n', cursor);
    if (next == std::string::npos) break;
    auto record = log.substr(cursor, next - cursor);
    if (!record.empty() && record.back() == '\r') record.pop_back();
    protocol.consume(record);
    cursor = next + 1;
  }
  return protocol;
}

#ifdef _WIN32
inline std::filesystem::path windowsVanityResult(const client::ClientExecutionState& state) {
  return pathFromUtf8(state.workspace) / ("vanity-result-" + std::to_string(state.vanityResumeOffset) + ".txt");
}

inline std::optional<vanityPreview::Protocol> readWindowsVanityEvidence(
    const client::ClientExecutionState& state, const std::filesystem::path& generation) {
  // The owned process writes this private log with FILE_SHARE_READ. Allow
  // its existing write handle, and read at most one bounded native range log.
  const auto path = generation / "engine.log";
  if (!windowsFile::safePath(path)) return {};
  windowsFile::Handle input(CreateFileW(path.c_str(), GENERIC_READ,
      FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
  if (!input.valid()) return {};
  constexpr std::size_t maximumLogSize = 2 * 1024 * 1024;
  std::string log;
  char buffer[4096];
  for (;;) {
    DWORD size = 0;
    if (!ReadFile(input.get(), buffer, sizeof(buffer), &size, nullptr)) return {};
    if (!size) break;
    if (log.size() + size > maximumLogSize) return {};
    log.append(buffer, size);
  }
  try {
    return windowsVanityEvidence(state, log);
  } catch (const std::exception&) {
    return {};
  }
}
#endif

} // namespace openpuzzle

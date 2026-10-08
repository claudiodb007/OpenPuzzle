#pragma once
#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace openpuzzle {
// This format is deliberately different from Linux boot-id/start-tick records.
// Windows requests carry the complete identity they authorize, not just a PID.
struct WindowsRuntimeRecord {
  int pid = 0;
  std::uint64_t creationTime = 0;
  static constexpr std::string_view platform = "windows-filetime-v1";
  bool operator==(const WindowsRuntimeRecord&) const = default;
  bool valid() const { return pid > 0 && creationTime > 0; }
  std::string encode() const {
    if (!valid()) return {};
    return std::to_string(pid) + '\n' + std::string(platform) + '\n' +
           std::to_string(creationTime) + '\n';
  }
  static std::optional<WindowsRuntimeRecord> parse(std::string_view text) {
    // Bound input and accept only three complete lines, optionally ending in LF.
    if (text.empty() || text.size() > 128) return std::nullopt;
    auto line = [&text]() -> std::optional<std::string_view> {
      const auto end = text.find('\n');
      const auto count = end == std::string_view::npos ? text.size() : end;
      auto value = text.substr(0, count);
      text.remove_prefix(count + (end == std::string_view::npos ? 0 : 1));
      if (!value.empty() && value.back() == '\r') value.remove_suffix(1);
      return value.empty() ? std::nullopt : std::optional(value);
    };
    const auto pidText = line(), tag = line(), timeText = line();
    if (!pidText || !tag || !timeText || *tag != platform || !text.empty()) return std::nullopt;
    WindowsRuntimeRecord record;
    const auto pid = std::from_chars(pidText->data(), pidText->data() + pidText->size(), record.pid);
    const auto time = std::from_chars(timeText->data(), timeText->data() + timeText->size(), record.creationTime);
    if (pid.ec != std::errc{} || pid.ptr != pidText->data() + pidText->size() ||
        time.ec != std::errc{} || time.ptr != timeText->data() + timeText->size() || !record.valid()) return std::nullopt;
    return record;
  }
};
} // namespace openpuzzle

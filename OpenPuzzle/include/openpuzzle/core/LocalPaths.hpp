#pragma once
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace openpuzzle {
inline std::filesystem::path pathFromUtf8(std::string_view value) {
  return std::filesystem::path(std::u8string(value.begin(), value.end()));
}
inline std::string pathUtf8(const std::filesystem::path& path) {
  const auto text = path.u8string();
  return {text.begin(), text.end()};
}
namespace localPaths {
#ifdef _WIN32
inline std::filesystem::path root() {
  const DWORD size = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
  if (!size) throw std::runtime_error("LOCALAPPDATA is unavailable");
  std::wstring value(size, L'\0');
  const DWORD count = GetEnvironmentVariableW(L"LOCALAPPDATA", value.data(), size);
  if (!count || count >= size) throw std::runtime_error("LOCALAPPDATA changed or is unavailable");
  value.resize(count);
  const std::filesystem::path path(value);
  if (!path.is_absolute()) throw std::runtime_error("LOCALAPPDATA must be absolute");
  return path / L"OpenPuzzle";
}
inline std::filesystem::path configDirectory() { return root() / L"config"; }
inline std::filesystem::path dataDirectory() { return root() / L"data"; }
#else
inline std::filesystem::path home() {
  const char* value = std::getenv("HOME");
  return value ? std::filesystem::path(value) : std::filesystem::current_path();
}
inline std::filesystem::path configDirectory() { return home() / ".config" / "OpenPuzzle"; }
inline std::filesystem::path dataDirectory() { return home() / ".local" / "share" / "OpenPuzzle"; }
#endif
}
}

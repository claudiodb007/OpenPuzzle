#include "openpuzzle/services/PuzzleMetadataCatalog.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

namespace openpuzzle {

namespace {

bool extractString(
    const std::string &json,
    const std::string &key,
    std::string &value) {
  const std::regex expression(
      "\\\"" + key +
      "\\\"\\s*:\\s*\\\"((?:\\\\.|[^\\\"\\\\])*)\\\"");

  std::smatch match;
  if (!std::regex_search(json, match, expression))
    return false;

  value = match[1].str();
  return true;
}

bool extractInteger(
    const std::string &json,
    const std::string &key,
    int &value) {
  const std::regex expression(
      "\\\"" + key + "\\\"\\s*:\\s*(-?[0-9]+)");

  std::smatch match;
  if (!std::regex_search(json, match, expression))
    return false;

  try {
    value = std::stoi(match[1].str());
    return true;
  } catch (...) {
    return false;
  }
}

bool isHex(const std::string &value, std::size_t exactLength = 0) {
  if (value.empty() || (exactLength != 0 && value.size() != exactLength))
    return false;

  return std::all_of(
      value.begin(), value.end(),
      [](unsigned char character) { return std::isxdigit(character) != 0; });
}

std::string normalizedHex(std::string value) {
  const auto first = value.find_first_not_of('0');
  if (first == std::string::npos)
    return "0";

  value.erase(0, first);
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
      });
  return value;
}

bool validKeyspace(const std::string &value) {
  const auto separator = value.find(':');
  if (separator == std::string::npos ||
      value.find(':', separator + 1) != std::string::npos)
    return false;

  const auto start = value.substr(0, separator);
  const auto end = value.substr(separator + 1);
  if (!isHex(start) || !isHex(end))
    return false;

  const auto normalizedStart = normalizedHex(start);
  const auto normalizedEnd = normalizedHex(end);
  return normalizedStart.size() < normalizedEnd.size() ||
         (normalizedStart.size() == normalizedEnd.size() &&
          normalizedStart < normalizedEnd);
}

bool validCompressedPublicKey(const std::string &value) {
  return isHex(value, 66) &&
         (value.rfind("02", 0) == 0 || value.rfind("03", 0) == 0);
}

bool validMainnetAddress(const std::string &value) {
  static const std::regex expression(
      "^1[123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz]{25,34}$");
  return std::regex_match(value, expression);
}

std::vector<fs::path> candidateDirectories() {
  std::vector<fs::path> directories;

  if (const char *overrideDir = std::getenv("OPENPUZZLE_PUZZLE_DIR");
      overrideDir && *overrideDir) {
    directories.emplace_back(overrideDir);
  }

#ifdef OPENPUZZLE_SOURCE_PUZZLE_DIR
  directories.emplace_back(OPENPUZZLE_SOURCE_PUZZLE_DIR);
#endif

  directories.emplace_back("/usr/share/OpenPuzzle/puzzles");
  directories.emplace_back("/usr/share/openpuzzle/puzzles");

  return directories;
}

} // namespace

std::optional<std::string>
PuzzleMetadataCatalog::readPuzzleFile(int puzzle) {
  if (puzzle <= 0)
    return std::nullopt;

  const auto filename = std::to_string(puzzle) + ".json";

  for (const auto &directory : candidateDirectories()) {
    const auto path = directory / filename;
    std::ifstream input(path);

    if (!input)
      continue;

    std::ostringstream content;
    content << input.rdbuf();

    if (input.bad())
      continue;

    return content.str();
  }

  return std::nullopt;
}

std::optional<PuzzleExecutionMetadata>
PuzzleMetadataCatalog::parse(
    int expectedPuzzle,
    const std::string &json) {
  PuzzleExecutionMetadata metadata;

  if (!extractInteger(json, "number", metadata.puzzle) || metadata.puzzle <= 0)
    return std::nullopt;

  if (expectedPuzzle > 0 && metadata.puzzle != expectedPuzzle)
    return std::nullopt;

  if (!extractString(json, "search_mode", metadata.searchMode))
    metadata.searchMode = "linear";

  if (metadata.searchMode != "linear" && metadata.searchMode != "kangaroo")
    return std::nullopt;

  extractString(json, "address", metadata.address);
  extractString(json, "hash160", metadata.hash160);
  extractString(json, "keyspace", metadata.keyspace);
  extractString(json, "public_key", metadata.publicKey);
  extractString(json, "required_backend", metadata.requiredBackend);

  if (metadata.searchMode == "kangaroo" &&
      (!validMainnetAddress(metadata.address) ||
       !isHex(metadata.hash160, 40) ||
       !validKeyspace(metadata.keyspace) ||
       !validCompressedPublicKey(metadata.publicKey) ||
       metadata.requiredBackend != "cuda")) {
    return std::nullopt;
  }

  return metadata;
}

std::optional<PuzzleExecutionMetadata>
PuzzleMetadataCatalog::load(int puzzle) {
  const auto json = readPuzzleFile(puzzle);
  if (!json)
    return std::nullopt;

  return parse(puzzle, *json);
}

} // namespace openpuzzle

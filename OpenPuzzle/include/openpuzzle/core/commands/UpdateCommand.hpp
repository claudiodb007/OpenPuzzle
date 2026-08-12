#pragma once

#include <optional>
#include <string>
#include <vector>

namespace openpuzzle {

struct UpdateRelease {
  std::string sha256;
  std::string filename;
  std::string version;
};

struct UpdateOptions {
  bool checkOnly = false;
  bool downloadOnly = false;
  bool safe = false;
};

class UpdateCommand {
public:
  int run(const std::vector<std::string>& args) const;

  static std::optional<UpdateOptions>
  parseOptions(
      const std::vector<std::string>& args,
      std::string& error);

  static std::optional<UpdateRelease>
  parseReleaseManifest(
      const std::string& manifest,
      std::string& error);

  static int compareVersions(
      const std::string& left,
      const std::string& right);
};

} // namespace openpuzzle

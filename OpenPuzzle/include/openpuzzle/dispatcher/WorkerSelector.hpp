#pragma once

#include "openpuzzle/models/Models.hpp"

#include <optional>
#include <string>

namespace openpuzzle {

class Database;

class WorkerSelector {
public:
  explicit WorkerSelector(Database& database);

  std::optional<WorkerRecord> selectIdleWorker();

  std::optional<WorkerRecord> selectIdleWorker(
      const std::string& engine,
      const std::string& backend);

private:
  Database& database_;

  static bool betterCandidate(
      const WorkerRecord& candidate,
      const WorkerRecord& current);
};

} // namespace openpuzzle

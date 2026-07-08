#pragma once

#include "openpuzzle/database/Database.hpp"
#include "openpuzzle/runtime/Execution.hpp"

#include <string>

namespace openpuzzle {

class ExecutionRepository {
public:
  explicit ExecutionRepository(Database& database);

  int create(int jobId,
             const std::string& workspace,
             const std::string& command,
             const std::string& state);

  bool finish(const Execution& execution);

private:
  Database& database_;
};

} // namespace openpuzzle

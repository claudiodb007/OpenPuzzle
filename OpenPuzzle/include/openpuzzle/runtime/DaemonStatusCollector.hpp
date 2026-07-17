#pragma once

#include "openpuzzle/runtime/DaemonStatus.hpp"

namespace openpuzzle {

class Database;

class DaemonStatusCollector {
public:
  explicit DaemonStatusCollector(Database& database);

  DaemonStatus collect() const;

private:
  Database& database_;
};

} // namespace openpuzzle

#pragma once

#include <string>

namespace openpuzzle {

class Database;

struct ExecutionProcessMonitorSummary {
  int running = 0;
  int finished = 0;
  int failed = 0;
  int cancelled = 0;
  int missingPid = 0;
};

class ExecutionProcessMonitor {
public:
  explicit ExecutionProcessMonitor(Database& database);

  ExecutionProcessMonitorSummary poll();

private:
  Database& database_;

  static int readPid(const std::string& workspace);
  static int readExitCode(const std::string& workspace);
  static bool processExists(int pid);
};

} // namespace openpuzzle

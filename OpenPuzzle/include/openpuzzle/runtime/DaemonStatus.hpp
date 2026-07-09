#pragma once

namespace openpuzzle {

struct DaemonStatus {
  int reservedJobs = 0;
  int runningJobs = 0;
  int runningExecutions = 0;
  int workers = 0;
};

} // namespace openpuzzle

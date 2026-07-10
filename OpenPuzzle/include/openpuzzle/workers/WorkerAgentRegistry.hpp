#pragma once

#include "openpuzzle/workers/WorkerAgent.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace openpuzzle {

class WorkerAgentRegistry {
public:
  bool add(WorkerAgent agent);

  WorkerAgent* find(int workerId);
  const WorkerAgent* find(int workerId) const;

  WorkerAgent* acquireIdle();
  WorkerAgent* acquireIdle(const std::string& engine,
                           const std::string& backend);

  std::vector<WorkerAgent*> all();
  std::vector<const WorkerAgent*> all() const;

  std::size_t count() const;
  void clear();

private:
  std::vector<WorkerAgent> agents_;
};

} // namespace openpuzzle

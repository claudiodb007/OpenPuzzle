#pragma once

#include "openpuzzle/workers/Worker.hpp"
#include "openpuzzle/workers/WorkerState.hpp"

#include <optional>
#include <vector>

namespace openpuzzle {

class WorkerRegistry {
public:
  void add(const WorkerState& state);

  std::optional<WorkerState> find(int workerId) const;

  std::vector<WorkerState> all() const;
  std::vector<WorkerState> online() const;
  std::vector<WorkerState> busy() const;
  std::vector<WorkerState> offline() const;

  bool update(const WorkerState& state);

  std::size_t count() const;
  void clear();

private:
  std::vector<Worker> workers_;
};

} // namespace openpuzzle

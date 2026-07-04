#pragma once

#include "openpuzzle/database/Database.hpp"

namespace openpuzzle {

class GpuProfileManager {
public:
  explicit GpuProfileManager(Database &database);

  bool save(const GpuProfileRecord &profile);

  std::optional<GpuProfileRecord> load(const std::string &gpuName,
                                       const std::string &backend,
                                       const std::string &engine);

  std::optional<GpuProfileRecord> chooseBest(const std::string &gpuName,
                                             const std::string &backend,
                                             const std::string &engine);

private:
  Database &database_;
};

} // namespace openpuzzle

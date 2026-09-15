#pragma once

#include "openpuzzle/engines/ISearchEngine.hpp"

#include <string>

namespace openpuzzle {

class KangarooEngine : public ISearchEngine {
public:
  explicit KangarooEngine(
      std::string executable,
      int ramLimitGiB = 0);

  EngineInfo info() const override;

  bool prepare() override;
  std::string buildCommand(
      const EngineLaunchRequest &request) const override;
  bool launch() override;
  bool stop() override;
  bool running() const override;

private:
  std::string executable_;
  int ramLimitGiB_ = 0;
  bool running_ = false;
};

} // namespace openpuzzle

#pragma once
#include "VanityPreviewEngine.hpp"
#include "openpuzzle/engines/VanityEngineConfiguration.hpp"
#include <memory>

namespace openpuzzle {
class VanityCommonEngine final : public ISearchEngine {
public:
  explicit VanityCommonEngine(VanityEngineConfiguration configuration);
  EngineInfo info() const override;
  bool prepare() override;
  std::string buildCommand(const EngineLaunchRequest&) const override;
  bool launch() override { return false; }
  bool stop() override { return false; }
  bool running() const override { return false; }
private:
  VanityEngineConfiguration configuration_;
};

// Reads the existing adapter's bounded BEGIN/PROGRESS/RESULT records. A
// malformed or appended protocol record permanently invalidates this parser.
class VanityCommonProgressParser final : public EngineOutputParser {
public:
  std::optional<ExecutionProgress> parseLine(const std::string&) override;
  bool complete(const std::string& expected) const;
private:
  std::unique_ptr<vanityPreview::VanityPreviewProgressParser> parser_;
  std::string total_;
  bool invalid_ = false, finished_ = false;
};

bool vanityCommonCompletionProof(const std::string& workspace, const std::string& expected);
bool vanityCommonProgressWithinCoverage(const std::string& workspace, const std::string& checked);
}
